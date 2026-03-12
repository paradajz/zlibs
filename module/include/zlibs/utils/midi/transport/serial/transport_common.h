/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <optional>

namespace zlibs::utils::midi::serial
{
    /**
     * @brief Hardware abstraction interface for serial MIDI transport.
     */
    class Hwa
    {
        public:
        virtual ~Hwa() = default;

        /**
         * @brief Initializes serial MIDI hardware backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init() = 0;

        /**
         * @brief Deinitializes serial MIDI hardware backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool deinit() = 0;

        /**
         * @brief Writes one serial MIDI byte.
         *
         * @param data MIDI byte to transmit.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write(uint8_t data) = 0;

        /**
         * @brief Reads one serial MIDI byte.
         *
         * @return Read byte on success, or `std::nullopt` when no byte is available.
         */
        virtual std::optional<uint8_t> read() = 0;
    };
}    // namespace zlibs::utils::midi::serial
