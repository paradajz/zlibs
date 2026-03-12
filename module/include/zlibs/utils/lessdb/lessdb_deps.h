/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lessdb_common.h"

#include <optional>

namespace zlibs::utils::lessdb
{
    /**
     * @brief Hardware abstraction interface for database storage.
     */
    class Hwa
    {
        public:
        Hwa()          = default;
        virtual ~Hwa() = default;

        /**
         * @brief Initializes hardware resources.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init() = 0;

        /**
         * @brief Returns the total storage size in bytes.
         *
         * @return Storage size in bytes.
         */
        virtual uint32_t size() = 0;

        /**
         * @brief Clears all storage contents.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool clear() = 0;

        /**
         * @brief Reads a value from storage at the given address.
         *
         * @param address Storage address to read from.
         * @param type    Parameter type determining the read width.
         *
         * @return Read value on success, otherwise `std::nullopt`.
         */
        virtual std::optional<uint32_t> read(uint32_t address, SectionParameterType type) = 0;

        /**
         * @brief Writes a value to storage at the given address.
         *
         * @param address Storage address to write to.
         * @param value   Value to write.
         * @param type    Parameter type determining the write width.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write(uint32_t address, uint32_t value, SectionParameterType type) = 0;
    };
}    // namespace zlibs::utils::lessdb
