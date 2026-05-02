/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "emueeprom_deps.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace zlibs::utils::emueeprom
{
    /**
     * @brief Flash-backed EEPROM emulation using two rotating log pages.
     *
     * Each logical address stores one 16-bit value. The implementation keeps a
     * RAM mirror of the latest values so reads are fast and page transfers do
     * not need to rescan old entries.
     */
    class EmuEeprom
    {
        public:
        /**
         * @brief Creates an emulated EEPROM instance bound to a concrete flash backend.
         *
         * The concrete HWA must expose compile-time page-size constants for all
         * three pages. This constructor enforces that they all match
         * `CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE`.
         *
         * @tparam ConcreteHwa Concrete backend type derived from `Hwa`.
         *
         * @param hwa Hardware abstraction used for flash-page access.
         */
        template<typename ConcreteHwa>
            requires std::derived_from<ConcreteHwa, Hwa>
        explicit EmuEeprom(ConcreteHwa& hwa)
            : _hwa(hwa)
            , _write_block_size(ConcreteHwa::WRITE_BLOCK_SIZE)
            , _read_block_size(INTERNAL_BLOCK_SIZE)
            , _max_address(static_cast<uint16_t>(max_exclusive_address_for(ConcreteHwa::WRITE_BLOCK_SIZE)))
            , _entries_per_block(ConcreteHwa::WRITE_BLOCK_SIZE / SERIALIZED_ENTRY_SIZE)
        {
            static_assert(requires {
                              { ConcreteHwa::WRITE_BLOCK_SIZE } -> std::convertible_to<size_t>;
                              { ConcreteHwa::PAGE_1_SIZE } -> std::convertible_to<size_t>;
                              { ConcreteHwa::PAGE_2_SIZE } -> std::convertible_to<size_t>;
                              { ConcreteHwa::FACTORY_SIZE } -> std::convertible_to<size_t>; }, "EmuEeprom HWA must define WRITE_BLOCK_SIZE, PAGE_1_SIZE, PAGE_2_SIZE, and FACTORY_SIZE.");

            static_assert(ConcreteHwa::PAGE_1_SIZE == CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE,
                          "EmuEeprom page 1 size must match CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE.");
            static_assert(ConcreteHwa::PAGE_2_SIZE == CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE,
                          "EmuEeprom page 2 size must match CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE.");
            static_assert(ConcreteHwa::FACTORY_SIZE == CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE,
                          "EmuEeprom factory page size must match CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE.");
            static_assert(ConcreteHwa::WRITE_BLOCK_SIZE >= SERIALIZED_ENTRY_SIZE,
                          "EmuEeprom write block must be at least one entry wide.");
            static_assert((ConcreteHwa::WRITE_BLOCK_SIZE % SERIALIZED_ENTRY_SIZE) == 0,
                          "EmuEeprom write block must be a multiple of the serialized entry size.");
            static_assert(ConcreteHwa::WRITE_BLOCK_SIZE <= INTERNAL_BLOCK_SIZE,
                          "EmuEeprom backend write block exceeds internal block size.");
            static_assert((INTERNAL_BLOCK_SIZE % ConcreteHwa::WRITE_BLOCK_SIZE) == 0,
                          "EmuEeprom internal block size must be a multiple of the backend write block.");
            static_assert((CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE % INTERNAL_BLOCK_SIZE) == 0,
                          "EmuEeprom page size must be a multiple of the internal block size.");
            static_assert(has_logical_address_space_for(ConcreteHwa::WRITE_BLOCK_SIZE),
                          "EmuEeprom page size must leave room for at least one logical address.");
        }

        /**
         * @brief Initializes the backend and repairs page-state combinations if needed.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init();

        /**
         * @brief Reads one 16-bit value from a logical EEPROM address.
         *
         * Valid logical addresses are in the range `[0, max_address())`.
         *
         * @param address Logical EEPROM address.
         * @param data Reference populated with the value on success.
         *
         * @return Read result status.
         */
        ReadStatus read(uint32_t address, uint16_t& data);

        /**
         * @brief Writes one 16-bit value to a logical EEPROM address.
         *
         * Valid logical addresses are in the range `[0, max_address())`.
         *
         * @param entry Logical EEPROM entry to write.
         * @param cache_only When `true`, only the RAM cache is updated; use
         *                   `flush()` later to persist it.
         *
         * @return Write result status.
         */
        WriteStatus write(Entry entry, bool cache_only = false);

        /**
         * @brief Erases runtime pages and reinitializes them for use.
         *
         * @param format_factory_page When `true`, the factory page is erased as
         *                            part of the format operation too.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool format(bool format_factory_page = false);

        /**
         * @brief Stores the currently active runtime page into the factory page.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool store_to_factory();

        /**
         * @brief Restores runtime pages from the factory page.
         *
         * Runtime pages are reformatted first, then a valid factory page is
         * copied into page 1 and page 2 is marked formatted.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool restore_from_factory();

        /**
         * @brief Returns the current state of a flash page.
         *
         * @param page Page role to inspect.
         *
         * @return Page status.
         */
        PageStatus page_status(Page page);

        /**
         * @brief Moves cached values to the alternate runtime page.
         *
         * @return Transfer result status.
         */
        WriteStatus page_transfer();

        /**
         * @brief Returns the exclusive upper bound of valid logical addresses.
         *
         * @return Number of addressable 16-bit values.
         */
        uint16_t max_address() const
        {
            return _max_address;
        }

        /**
         * @brief Makes all known EEPROM state durable.
         *
         * This persists RAM-only cache changes and flushes a partial backend
         * write block if one is pending.
         *
         * @return `true` if all pending data was persisted, otherwise `false`.
         */
        bool flush();

        private:
        enum class PageOperation : uint8_t
        {
            Read,
            Write,
        };

        /**
         * @brief Cached next free log offset and the page it belongs to.
         */
        struct NextWriteOffsetCache
        {
            Page     page   = Page::Page1;
            uint32_t offset = 0;
        };

        using EntryAddress = decltype(Entry::address);
        using EntryValue   = decltype(Entry::value);

        static constexpr size_t INTERNAL_BLOCK_SIZE            = 256;
        static constexpr size_t MIN_INTERNAL_ENTRY_BLOCK_COUNT = 3;
        static constexpr size_t MAX_CACHE_ENTRY_COUNT =
            CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / SERIALIZED_ENTRY_SIZE;

        static_assert(std::is_unsigned_v<EntryAddress>, "EmuEeprom entry address must be an unsigned integer.");
        static_assert(std::is_unsigned_v<EntryValue>, "EmuEeprom entry value must be an unsigned integer.");
        static_assert(CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE % SERIALIZED_ENTRY_SIZE == 0,
                      "EmuEeprom page size must be a multiple of the serialized entry size.");
        static_assert(MAX_CACHE_ENTRY_COUNT > 0, "EmuEeprom page size must have room for at least one entry.");
        static_assert(MAX_CACHE_ENTRY_COUNT <= std::numeric_limits<uint16_t>::max(),
                      "EmuEeprom logical address space must fit in 16 bits.");

        Hwa&                                        _hwa;
        const size_t                                _write_block_size;
        const size_t                                _read_block_size;
        const uint16_t                              _max_address;
        const size_t                                _entries_per_block;
        std::array<uint16_t, MAX_CACHE_ENTRY_COUNT> _cache                   = {};
        std::array<uint8_t, INTERNAL_BLOCK_SIZE>    _write_block_buffer      = {};
        std::array<PageStatus, 3>                   _page_status_cache       = { PageStatus::Erased, PageStatus::Erased, PageStatus::Erased };
        NextWriteOffsetCache                        _next_write_offset_cache = {};
        Page                                        _write_block_page        = Page::Page1;
        uint32_t                                    _write_block_offset      = 0;
        size_t                                      _write_block_entries     = 0;
        bool                                        _write_block_dirty       = false;
        bool                                        _cache_dirty             = false;

        /**
         * @brief Calculates the exclusive upper bound for logical addresses.
         *
         * The status record uses the erased address value as an internal
         * sentinel. The returned value itself is not a valid address; addresses
         * are valid only while `address < max_address()`, so the
         * returned bound may equal the reserved status address while still
         * excluding it. A few backend write blocks are also kept out of the
         * exposed address range so page status transitions and transfers
         * always have room for their internal records.
         *
         * @param write_block_size Backend write-block size in bytes.
         *
         * @return Exclusive upper bound for valid logical addresses. The returned
         *         value itself is not a valid address.
         */
        static constexpr size_t max_exclusive_address_for(size_t write_block_size)
        {
            constexpr size_t PAGE_ENTRY_COUNT          = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / SERIALIZED_ENTRY_SIZE;
            const size_t     entries_per_backend_block = write_block_size / SERIALIZED_ENTRY_SIZE;
            const size_t     internal_entry_count      = MIN_INTERNAL_ENTRY_BLOCK_COUNT * entries_per_backend_block;

            if (PAGE_ENTRY_COUNT <= internal_entry_count)
            {
                return 0;
            }

            const size_t max_exclusive_address = PAGE_ENTRY_COUNT - internal_entry_count;

            return max_exclusive_address > RESERVED_STATUS_ADDRESS ? RESERVED_STATUS_ADDRESS : max_exclusive_address;
        }

        /**
         * @brief Checks that page size leaves room for user data after internal records.
         *
         * @param write_block_size Backend write-block size in bytes.
         *
         * @return `true` when at least one logical address remains available.
         */
        static constexpr bool has_logical_address_space_for(size_t write_block_size)
        {
            return max_exclusive_address_for(write_block_size) > 0;
        }

        /**
         * @brief Resolves the page currently used for read or write operations.
         *
         * @param operation Requested access type.
         * @param page Reference populated with the selected page on success.
         *
         * @return `true` when a suitable page is found, otherwise `false`.
         */
        bool find_valid_page(PageOperation operation, Page& page);

        /**
         * @brief Appends one logical EEPROM entry to flash or updates only the RAM cache.
         *
         * @param entry Logical EEPROM entry to append.
         * @param cache_only When `true`, updates only the RAM cache.
         *
         * @return Write result status.
         */
        WriteStatus write_internal(Entry entry, bool cache_only = false);

        /**
         * @brief Stages one logical entry for writing at a page offset.
         *
         * The entry is serialized into the pending backend write-block buffer.
         *
         * @param page Page to write to.
         * @param offset Byte offset inside the page.
         * @param entry Logical EEPROM entry to stage.
         *
         * @return Write result status.
         */
        WriteStatus write_entry(Page page, uint32_t offset, Entry entry);

        /**
         * @brief Finds the next erased backend write block in a page.
         *
         * @param page Page to scan.
         *
         * @return Erased block offset, or an empty optional when no free block is found.
         */
        std::optional<uint32_t> find_next_free_offset(Page page);

        /**
         * @brief Refreshes one cached page status from flash.
         *
         * @param page Page whose status should be refreshed.
         *
         * @return `true` when flash was scanned successfully, otherwise `false`.
         */
        bool refresh_page_status(Page page);

        /**
         * @brief Returns one cached page status without touching flash.
         *
         * @param page Page whose cached status should be returned.
         *
         * @return Cached page status.
         */
        PageStatus cached_page_status(Page page) const;

        /**
         * @brief Updates cached page status without touching flash.
         *
         * @param page Page whose cached status should be updated.
         * @param status New cached page status.
         */
        void set_cached_page_status(Page page, PageStatus status);

        /**
         * @brief Refreshes the cached next free log offset from flash.
         *
         * @param page Page to scan.
         *
         * @return `true` when flash was scanned successfully, otherwise `false`.
         */
        bool refresh_next_write_offset(Page page);

        /**
         * @brief Returns cached next free log offset and associated page without touching flash.
         *
         * @return Cached next free log offset and associated page.
         */
        NextWriteOffsetCache cached_next_write_offset() const;

        /**
         * @brief Updates cached next free log offset without touching flash.
         *
         * @param page Page associated with the offset.
         * @param offset New cached next free log offset.
         */
        void set_cached_next_write_offset(Page page, uint32_t offset);

        /**
         * @brief Appends a page status entry to the regular log.
         *
         * @param page Page to write to.
         * @param status Page status to append.
         *
         * @return `true` when the status entry was written and flushed, otherwise `false`.
         */
        bool write_page_status(Page page, PageStatus status);

        /**
         * @brief Encodes a page status into a reserved status entry's 16-bit value.
         *
         * @param status Page status to encode.
         *
         * @return Encoded 16-bit status value.
         */
        static EntryValue serialize_page_status(PageStatus status);

        /**
         * @brief Decodes a reserved status entry's 16-bit value into a page status.
         *
         * @param value Encoded 16-bit status value.
         *
         * @return Decoded page status, or an empty optional when the value is not a valid status encoding.
         */
        static std::optional<PageStatus> deserialize_page_status(EntryValue value);

        /**
         * @brief Reads and decodes one logical entry from a page offset.
         *
         * @param page Page to read from.
         * @param offset Byte offset inside the page.
         *
         * @return Decoded entry, or an empty optional when the entry cannot be read.
         */
        std::optional<Entry> read_entry(Page page, uint32_t offset);

        /**
         * @brief Flushes a partially filled backend write block.
         *
         * @return `true` when no write is pending or the pending block was written, otherwise `false`.
         */
        bool flush_write_block();

        /**
         * @brief Clears pending backend write-block state without writing it.
         */
        void clear_write_block();

        /**
         * @brief Rebuilds the RAM cache from the active flash page.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool cache();
    };
}    // namespace zlibs::utils::emueeprom
