/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "emueeprom_common.h"

#include <optional>
#include <span>

namespace zlibs::utils::emueeprom
{
    /**
     * @brief Hardware abstraction interface for flash-page operations.
     */
    class Hwa
    {
        public:
        Hwa()          = default;
        virtual ~Hwa() = default;

        /**
         * @brief Initializes the flash backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init() = 0;

        /**
         * @brief Erases one flash page.
         *
         * @param page Target page role.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool erase_page(Page page) = 0;

        /**
         * @brief Writes bytes at the given page offset.
         *
         * Implementations must accept the write size and alignment passed to
         * their owning `EmuEeprom` instance.
         *
         * @param page Target page role.
         * @param offset Byte offset within the page.
         * @param data Bytes to write.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write(Page page, uint32_t offset, std::span<const uint8_t> data) = 0;

        /**
         * @brief Reads bytes at the given page offset.
         *
         * Reads must accept offsets aligned to the configured write-block size;
         * the requested size can be one or more complete write blocks.
         *
         * @param page Target page role.
         * @param offset Byte offset within the page.
         * @param data Output buffer populated with bytes read from storage.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool read(Page page, uint32_t offset, std::span<uint8_t> data) = 0;
    };
}    // namespace zlibs::utils::emueeprom
