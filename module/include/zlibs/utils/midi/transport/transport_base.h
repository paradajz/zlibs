/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/midi/midi_common.h"

#include <optional>

namespace zlibs::utils::midi
{
    /**
     * @brief Interface for MIDI thru output sinks.
     */
    class Thru
    {
        public:
        virtual ~Thru() = default;

        /**
         * @brief Writes one Universal MIDI Packet to the thru sink.
         *
         * @param packet UMP to forward.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write(const midi_ump& packet) = 0;
    };

    /**
     * @brief Transport interface used by `midi::Base` for RX/TX.
     */
    class Transport : public Thru
    {
        public:
        ~Transport() override = default;

        /**
         * @brief Initializes the transport backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init() = 0;

        /**
         * @brief Deinitializes the transport backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool deinit() = 0;

        /**
         * @brief Writes one Universal MIDI Packet through the transport backend.
         *
         * @param packet UMP to transmit.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool write(const midi_ump& packet) override = 0;

        /**
         * @brief Reads one Universal MIDI Packet from the transport backend.
         *
         * @return Read packet on success, or `std::nullopt` when no packet is available.
         */
        virtual std::optional<midi_ump> read() = 0;
    };
}    // namespace zlibs::utils::midi
