/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "internal.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace zlibs::utils::emueeprom
{
    /**
     * @brief Owns a fixed-size logical EEPROM instance and its internal storage.
     *
     * @tparam PageSize Physical size of each EmuEEPROM page in bytes.
     * @tparam WriteBlockSize Backend write-block size in bytes.
     * @tparam LogicalAddressCount Number of logical EEPROM addresses exposed
     *                             to users of this instance. Defaults to the
     *                             maximum supported by the page and write-block
     *                             sizes.
     */
    template<size_t PageSize, size_t WriteBlockSize, size_t LogicalAddressCount = address_count_for(PageSize, WriteBlockSize)>
    class EmuEeprom
    {
        public:
        static_assert(LogicalAddressCount > 0, "EmuEeprom logical address count must be greater than 0.");
        static_assert(LogicalAddressCount <= std::numeric_limits<uint16_t>::max(),
                      "EmuEeprom logical address count must fit in 16 bits.");
        static_assert(PageSize > 0, "EmuEeprom page size must be greater than 0.");
        static_assert((PageSize % SERIALIZED_ENTRY_SIZE) == 0,
                      "EmuEeprom page size must be a multiple of the serialized entry size.");
        static_assert((PageSize % INTERNAL_BLOCK_SIZE) == 0,
                      "EmuEeprom page size must be a multiple of the internal block size.");
        static_assert(WriteBlockSize >= SERIALIZED_ENTRY_SIZE,
                      "EmuEeprom write block must be at least one entry wide.");
        static_assert((WriteBlockSize % SERIALIZED_ENTRY_SIZE) == 0,
                      "EmuEeprom write block must be a multiple of the serialized entry size.");
        static_assert(WriteBlockSize <= INTERNAL_BLOCK_SIZE,
                      "EmuEeprom backend write block exceeds internal block size.");
        static_assert((INTERNAL_BLOCK_SIZE % WriteBlockSize) == 0,
                      "EmuEeprom internal block size must be a multiple of the backend write block.");
        static_assert(address_count_for(PageSize, WriteBlockSize) > 0,
                      "EmuEeprom page size must leave room for at least one logical address.");
        static_assert(LogicalAddressCount <= address_count_for(PageSize, WriteBlockSize),
                      "EmuEeprom logical address count exceeds physical journal capacity.");

        /**
         * @brief Creates a fixed-capacity EEPROM instance bound to a concrete flash backend.
         *
         * @tparam ConcreteHwa Concrete backend type derived from `Hwa`.
         *
         * @param hwa Hardware abstraction used for flash-page access.
         */
        template<typename ConcreteHwa>
            requires std::derived_from<ConcreteHwa, Hwa>
        explicit EmuEeprom(ConcreteHwa& hwa)
            : _emueeprom(hwa, PageSize, WriteBlockSize, _cache)
        {
        }

        /**
         * @brief Initializes the backend and repairs page-state combinations if needed.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init()
        {
            return _emueeprom.init();
        }

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
        ReadStatus read(uint32_t address, uint16_t& data)
        {
            return _emueeprom.read(address, data);
        }

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
        WriteStatus write(Entry entry, bool cache_only = false)
        {
            return _emueeprom.write(entry, cache_only);
        }

        /**
         * @brief Erases runtime pages and reinitializes them for use.
         *
         * @param format_factory_page When `true`, the factory page is erased as
         *                            part of the format operation too.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool format(bool format_factory_page = false)
        {
            return _emueeprom.format(format_factory_page);
        }

        /**
         * @brief Stores the currently active runtime page into the factory page.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool store_to_factory()
        {
            return _emueeprom.store_to_factory();
        }

        /**
         * @brief Restores runtime pages from the factory page.
         *
         * Runtime pages are reformatted first, then a valid factory page is
         * copied into page 1 and page 2 is marked formatted.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool restore_from_factory()
        {
            return _emueeprom.restore_from_factory();
        }

        /**
         * @brief Returns the current state of a flash page.
         *
         * @param page Page role to inspect.
         *
         * @return Page status.
         */
        PageStatus page_status(Page page)
        {
            return _emueeprom.page_status(page);
        }

        /**
         * @brief Moves cached values to the alternate runtime page.
         *
         * @return Transfer result status.
         */
        WriteStatus page_transfer()
        {
            return _emueeprom.page_transfer();
        }

        /**
         * @brief Returns the exclusive upper bound of valid logical addresses.
         *
         * @return Number of addressable 16-bit values.
         */
        uint16_t max_address() const
        {
            return _emueeprom.max_address();
        }

        /**
         * @brief Makes all known EEPROM state durable.
         *
         * This persists RAM-only cache changes and flushes a partial backend
         * write block if one is pending.
         *
         * @return `true` if all pending data was persisted, otherwise `false`.
         */
        bool flush()
        {
            return _emueeprom.flush();
        }

        private:
        std::array<uint16_t, LogicalAddressCount> _cache = {};
        internal::EmuEepromInternal               _emueeprom;
    };
}    // namespace zlibs::utils::emueeprom
