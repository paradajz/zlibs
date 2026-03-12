/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace zlibs::utils::midi::ble
{
    /**
     * @brief BLE-MIDI packet wrapper.
     */
    struct Packet
    {
        std::array<uint8_t, CONFIG_ZLIBS_UTILS_MIDI_BLE_MAX_PACKET_SIZE> data = {};
        size_t                                                           size = 0;
    };

    /**
     * @brief Hardware abstraction interface for BLE-MIDI transport.
     */
    class Hwa
    {
        public:
        virtual ~Hwa() = default;

        /**
         * @brief Initializes BLE-MIDI hardware backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init() = 0;

        /**
         * @brief Deinitializes BLE-MIDI hardware backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool deinit() = 0;

        /**
         * @brief Writes one encoded BLE-MIDI packet.
         *
         * @param packet Packet to transmit.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write(Packet& packet) = 0;

        /**
         * @brief Reads one encoded BLE-MIDI packet.
         *
         * @return Read packet on success, or `std::nullopt` when no packet is available.
         */
        virtual std::optional<Packet> read() = 0;
    };
}    // namespace zlibs::utils::midi::ble
