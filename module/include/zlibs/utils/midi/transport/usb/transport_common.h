/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <inttypes.h>
#include <optional>

namespace zlibs::utils::midi::usb
{
    /**
     * @brief USB-MIDI packet wrapper.
     */
    struct Packet
    {
        std::array<uint8_t, 4> data = {};

        /**
         * @brief Byte positions in a USB-MIDI packet.
         */
        enum PacketElement
        {
            Event,
            Data1,
            Data2,
            Data3,
        };
    };

    /**
     * @brief Hardware abstraction interface for USB-MIDI transport.
     */
    class Hwa
    {
        public:
        virtual ~Hwa() = default;

        /**
         * @brief Initializes USB MIDI hardware backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init() = 0;

        /**
         * @brief Deinitializes USB MIDI hardware backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool deinit() = 0;

        /**
         * @brief Writes one USB MIDI packet.
         *
         * @param packet Packet to transmit.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write(Packet& packet) = 0;

        /**
         * @brief Reads one USB MIDI packet.
         *
         * @return Read packet on success, or `std::nullopt` when no packet is available.
         */
        virtual std::optional<Packet> read() = 0;
    };
}    // namespace zlibs::utils::midi::usb
