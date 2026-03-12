/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <inttypes.h>
#include <optional>

namespace zlibs::utils::midi::serial
{
    /**
     * @brief Serial MIDI packet wrapper.
     */
    struct Packet
    {
        uint8_t data = 0;
    };

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
         * @brief Writes one serial MIDI packet.
         *
         * @param data Packet to transmit.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write(Packet& data) = 0;

        /**
         * @brief Reads one serial MIDI packet.
         *
         * @return Read packet on success, or `std::nullopt` when no packet is available.
         */
        virtual std::optional<Packet> read() = 0;
    };
}    // namespace zlibs::utils::midi::serial
