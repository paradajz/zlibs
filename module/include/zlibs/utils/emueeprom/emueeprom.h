/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "emueeprom_deps.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

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
         * @brief Creates an emulated EEPROM instance.
         *
         * @param hwa Hardware abstraction used for flash-page access.
         * @param use_factory_page When `true`, a valid factory page is copied
         *                         into page 1 during formatting.
         */
        EmuEeprom(Hwa& hwa, bool use_factory_page)
            : _hwa(hwa)
            , _use_factory_page(use_factory_page)
        {}

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
         * @param address Logical EEPROM address.
         * @param data Value to write.
         * @param cache_only When `true`, only the RAM cache is updated; use
         *                   `write_cache_to_flash()` later to persist it.
         *
         * @return Write result status.
         */
        WriteStatus write(uint32_t address, uint16_t data, bool cache_only = false);

        /**
         * @brief Erases runtime pages and reinitializes them for use.
         *
         * When `use_factory_page` is enabled and the factory page is valid, its
         * contents are copied into page 1.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool format();

        /**
         * @brief Returns the current header state of a flash page.
         *
         * @param page Page role to inspect.
         *
         * @return Page header status.
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
        uint32_t max_address() const;

        /**
         * @brief Flushes the RAM cache to flash by forcing a page transfer.
         */
        void write_cache_to_flash();

        private:
        enum class PageOperation : uint8_t
        {
            Read,
            Write,
        };

        static constexpr size_t   ENTRY_SIZE_BYTES   = sizeof(uint32_t);
        static constexpr size_t   PAGE_HEADER_SIZE   = sizeof(PageStatus);
        static constexpr uint16_t ERASED_VALUE       = 0xFFFF;
        static constexpr uint32_t EMPTY_WORD         = 0xFFFFFFFF;
        static constexpr uint32_t ADDRESS_SHIFT_BITS = std::numeric_limits<uint16_t>::digits;
        static constexpr size_t   ADDRESS_COUNT      = (CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / ENTRY_SIZE_BYTES) - 1;

        static_assert(CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE % ENTRY_SIZE_BYTES == 0,
                      "EmuEeprom page size must be a multiple of 4 bytes.");
        static_assert(ADDRESS_COUNT > 0, "EmuEeprom page size must have room for at least one entry.");
        static_assert(ADDRESS_COUNT <= std::numeric_limits<uint16_t>::max(),
                      "EmuEeprom logical address space must fit in 16 bits.");

        Hwa&                                _hwa;
        bool                                _use_factory_page;
        std::array<uint16_t, ADDRESS_COUNT> _cache                = {};
        uint32_t                            _next_offset_to_write = 0;

        /**
         * @brief Packs one logical EEPROM entry into the 32-bit flash word format.
         *
         * The upper 16 bits store the logical address and the lower 16 bits
         * store the value.
         *
         * @param address Logical EEPROM address.
         * @param data 16-bit value stored at that address.
         *
         * @return Packed 32-bit flash entry.
         */
        static constexpr uint32_t make_flash_entry(uint16_t address, uint16_t data)
        {
            return (static_cast<uint32_t>(address) << ADDRESS_SHIFT_BITS) | data;
        }

        /**
         * @brief Extracts the logical EEPROM address from one packed flash entry.
         *
         * @param entry Packed 32-bit flash entry.
         *
         * @return Stored logical address.
         */
        static constexpr uint16_t flash_entry_address(uint32_t entry)
        {
            return static_cast<uint16_t>((entry >> ADDRESS_SHIFT_BITS) & ERASED_VALUE);
        }

        /**
         * @brief Extracts the 16-bit value from one packed flash entry.
         *
         * @param entry Packed 32-bit flash entry.
         *
         * @return Stored 16-bit value.
         */
        static constexpr uint16_t flash_entry_value(uint32_t entry)
        {
            return static_cast<uint16_t>(entry & ERASED_VALUE);
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
         * @param address Logical EEPROM address.
         * @param data 16-bit value to store.
         * @param cache_only When `true`, updates only the RAM cache.
         *
         * @return Write result status.
         */
        WriteStatus write_internal(uint16_t address, uint16_t data, bool cache_only = false);

        /**
         * @brief Rebuilds the RAM cache from the active flash page.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool cache();
    };
}    // namespace zlibs::utils::emueeprom
