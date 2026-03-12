/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "emueeprom_common.h"

#include <optional>

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
         * @brief Writes one 32-bit word at the given page offset.
         *
         * @param page Target page role.
         * @param address Byte offset within the page.
         * @param data 32-bit word to write.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write_32(Page page, uint32_t address, uint32_t data) = 0;

        /**
         * @brief Reads one 32-bit word at the given page offset.
         *
         * @param page Target page role.
         * @param address Byte offset within the page.
         *
         * @return Read value on success, or `std::nullopt` when read fails.
         */
        virtual std::optional<uint32_t> read_32(Page page, uint32_t address) = 0;
    };
}    // namespace zlibs::utils::emueeprom
