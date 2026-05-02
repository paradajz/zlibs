/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "emueeprom_deps.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

namespace zlibs::utils::emueeprom::internal
{
    /**
     * @brief Internal EEPROM emulation implementation.
     *
     * Public users should instantiate EmuEeprom instead. This class
     * exists so the fixed-size owner can keep RAM storage compile-time sized
     * while the implementation itself remains non-template.
     */
    class EmuEepromInternal
    {
        public:
        /**
         * @brief Creates an internal implementation over owner-provided cache storage.
         *
         * @param hwa Flash backend.
         * @param page_size EmuEEPROM page size in bytes.
         * @param write_block_size Backend write-block size in bytes.
         * @param cache Logical value cache owned by the public wrapper.
         */
        EmuEepromInternal(Hwa& hwa, size_t page_size, size_t write_block_size, std::span<uint16_t> cache)
            : _hwa(hwa)
            , _page_size(page_size)
            , _write_block_size(write_block_size)
            , _read_block_size(INTERNAL_BLOCK_SIZE)
            , _max_address(static_cast<uint16_t>(cache.size()))
            , _entries_per_block(write_block_size / SERIALIZED_ENTRY_SIZE)
            , _cache(cache)
        {}

        /**
         * @brief Initializes backend state and repairs page headers if needed.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init();

        /**
         * @brief Reads one cached logical value.
         *
         * @param address Logical EEPROM address.
         * @param data Reference populated with the stored value on success.
         *
         * @return Read result status.
         */
        ReadStatus read(uint32_t address, uint16_t& data);

        /**
         * @brief Writes one logical value to cache and, unless requested otherwise, flash.
         *
         * @param entry Logical EEPROM entry to write.
         * @param cache_only When `true`, defer flash persistence until `flush()`.
         *
         * @return Write result status.
         */
        WriteStatus write(Entry entry, bool cache_only = false);

        /**
         * @brief Erases runtime pages and writes their initial page states.
         *
         * @param format_factory_page When `true`, erase the factory page too.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool format(bool format_factory_page = false);

        /**
         * @brief Copies the active runtime page into the factory page.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool store_to_factory();

        /**
         * @brief Rebuilds runtime pages from a valid factory page.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool restore_from_factory();

        /**
         * @brief Reads and returns the current cached status for a page.
         *
         * @param page Page to inspect.
         *
         * @return Page status, or `Invalid` when status refresh fails.
         */
        PageStatus page_status(Page page);

        /**
         * @brief Rewrites cached logical values to the inactive runtime page.
         *
         * @return Transfer result status.
         */
        WriteStatus page_transfer();

        /**
         * @brief Persists pending cache-only writes and a partial backend write block.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool flush();

        /**
         * @brief Returns the exclusive upper bound for logical addresses.
         *
         * @return Logical address count exposed by the owner cache.
         */
        uint16_t max_address() const
        {
            return _max_address;
        }

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

        static_assert(std::is_unsigned_v<EntryAddress>, "EmuEeprom entry address must be an unsigned integer.");
        static_assert(std::is_unsigned_v<EntryValue>, "EmuEeprom entry value must be an unsigned integer.");

        Hwa&                                     _hwa;
        const size_t                             _page_size;
        const size_t                             _write_block_size;
        const size_t                             _read_block_size;
        const uint16_t                           _max_address;
        const size_t                             _entries_per_block;
        std::span<uint16_t>                      _cache;
        std::array<uint8_t, INTERNAL_BLOCK_SIZE> _write_block_buffer      = {};
        std::array<PageStatus, 3>                _page_status_cache       = { PageStatus::Erased, PageStatus::Erased, PageStatus::Erased };
        NextWriteOffsetCache                     _next_write_offset_cache = {};
        Page                                     _write_block_page        = Page::Page1;
        uint32_t                                 _write_block_offset      = 0;
        size_t                                   _write_block_entries     = 0;
        bool                                     _write_block_dirty       = false;
        bool                                     _cache_dirty             = false;

        /**
         * @brief Resolves the page currently usable for a read or write operation.
         *
         * @param operation Requested operation type.
         * @param page Reference populated with the selected page on success.
         *
         * @return `true` when a suitable page is available, otherwise `false`.
         */
        bool find_valid_page(PageOperation operation, Page& page);

        /**
         * @brief Applies one logical write after public range checks.
         *
         * @param entry Logical EEPROM entry to write.
         * @param cache_only When `true`, update only RAM cache.
         *
         * @return Write result status.
         */
        WriteStatus write_internal(Entry entry, bool cache_only = false);

        /**
         * @brief Stages one serialized entry into the current backend write block.
         *
         * @param page Page to write.
         * @param offset Byte offset inside the page.
         * @param entry Logical entry to serialize.
         *
         * @return Write result status.
         */
        WriteStatus write_entry(Page page, uint32_t offset, Entry entry);

        /**
         * @brief Finds the next erased backend write block in a page.
         *
         * @param page Page to scan.
         *
         * @return Free byte offset, or empty optional when the page is full.
         */
        std::optional<uint32_t> find_next_free_offset(Page page);

        /**
         * @brief Refreshes the cached status for one page by scanning flash.
         *
         * @param page Page to refresh.
         *
         * @return `true` on successful scan, otherwise `false`.
         */
        bool refresh_page_status(Page page);

        /**
         * @brief Returns cached page status without touching flash.
         *
         * @param page Page to inspect.
         *
         * @return Cached page status.
         */
        PageStatus cached_page_status(Page page) const;

        /**
         * @brief Updates cached page status without touching flash.
         *
         * @param page Page to update.
         * @param status New cached status.
         */
        void set_cached_page_status(Page page, PageStatus status);

        /**
         * @brief Refreshes the cached next write offset for a page.
         *
         * @param page Page to scan.
         *
         * @return `true` on successful scan, otherwise `false`.
         */
        bool refresh_next_write_offset(Page page);

        /**
         * @brief Returns cached next write offset without touching flash.
         *
         * @return Cached next write offset and its page.
         */
        NextWriteOffsetCache cached_next_write_offset() const;

        /**
         * @brief Updates cached next write offset without touching flash.
         *
         * @param page Page associated with the offset.
         * @param offset Next free byte offset.
         */
        void set_cached_next_write_offset(Page page, uint32_t offset);

        /**
         * @brief Appends and flushes a page-status record.
         *
         * @param page Page to update.
         * @param status Status to write.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool write_page_status(Page page, PageStatus status);

        /**
         * @brief Encodes a page status for storage in the reserved status record.
         *
         * @param status Page status to encode.
         *
         * @return Serialized status value.
         */
        static EntryValue serialize_page_status(PageStatus status);

        /**
         * @brief Decodes a stored page-status value.
         *
         * @param value Serialized status value.
         *
         * @return Page status, or empty optional when the value is invalid.
         */
        static std::optional<PageStatus> deserialize_page_status(EntryValue value);

        /**
         * @brief Reads and decodes one serialized entry from flash.
         *
         * @param page Page to read.
         * @param offset Byte offset inside the page.
         *
         * @return Decoded entry, or empty optional on read/decode failure.
         */
        std::optional<Entry> read_entry(Page page, uint32_t offset);

        /**
         * @brief Writes a pending partial backend block if one exists.
         *
         * @return `true` when no block is pending or flush succeeds.
         */
        bool flush_write_block();

        /**
         * @brief Clears pending backend write-block state without writing it.
         */
        void clear_write_block();

        /**
         * @brief Rebuilds the RAM mirror from the active runtime page.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool cache();
    };
}    // namespace zlibs::utils::emueeprom::internal
