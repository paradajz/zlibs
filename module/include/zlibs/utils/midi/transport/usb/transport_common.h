/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/midi/midi_common.h"

#include <optional>

namespace zlibs::utils::midi::usb
{
    /**
     * @brief Hardware abstraction interface for USB-MIDI transport.
     */
    class Hwa
    {
        public:
        virtual ~Hwa() = default;

        /**
         * @brief Returns whether USB-MIDI hardware backend is available.
         *
         * @return `true` when the backend is supported, otherwise `false`.
         */
        virtual bool supported() = 0;

        /**
         * @brief Initializes the USB-MIDI hardware backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init() = 0;

        /**
         * @brief Deinitializes the USB-MIDI hardware backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool deinit() = 0;

        /**
         * @brief Writes one UMP packet through the USB-MIDI hardware backend.
         *
         * @param packet UMP packet to write.
         *
         * @return `true` when the packet was accepted, otherwise `false`.
         */
        virtual bool write(const midi_ump& packet) = 0;

        /**
         * @brief Reads one UMP packet from the USB-MIDI hardware backend.
         *
         * @return UMP packet when available, otherwise `std::nullopt`.
         */
        virtual std::optional<midi_ump> read() = 0;
    };
}    // namespace zlibs::utils::midi::usb
