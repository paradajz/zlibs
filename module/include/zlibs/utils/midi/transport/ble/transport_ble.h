/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "transport_common.h"
#include "zlibs/utils/midi/midi.h"

#include <array>
#include <cstddef>

namespace zlibs::utils::midi::ble
{
    /**
     * @brief BLE transport implementation of the MIDI base engine.
     */
    class Ble : public Base
    {
        public:
        /**
         * @brief Constructs a BLE MIDI interface backed by a BLE-MIDI packet hardware adapter.
         *
         * @param hwa Hardware abstraction used to send and receive encoded BLE-MIDI packets.
         */
        explicit Ble(Hwa& hwa)
            : Base(_transport)
            , _transport(*this)
            , _hwa(hwa)
        {}

        private:
        /**
         * @brief Internal BLE-MIDI packet transport.
         */
        class Transport : public midi::Transport
        {
            public:
            /**
             * @brief Constructs a BLE-MIDI transport bridge.
             *
             * @param ble Owning BLE MIDI instance.
             */
            explicit Transport(Ble& ble)
                : _ble(ble)
            {}

            bool                    init() override;
            bool                    deinit() override;
            bool                    write(const midi_ump& packet) override;
            std::optional<midi_ump> read() override;

            private:
            Ble&                 _ble;
            Packet               _tx_buffer           = {};
            Packet               _rx_packet           = {};
            size_t               _rx_packet_index     = 0;
            bool                 _rx_packet_active    = false;
            bool                 _rx_search_timestamp = true;
            bool                 _rx_sysex_active     = false;
            Midi1ByteToUmpParser _parser              = {};

            /**
             * @brief Starts a BLE-MIDI packet with timestamp bytes.
             */
            void start_packet();

            /**
             * @brief Writes one MIDI 1 byte into the active BLE-MIDI packet.
             */
            bool write_midi_byte(uint8_t data);

            /**
             * @brief Writes one complete encoded BLE-MIDI packet when it has payload.
             */
            bool flush_packet();

            /**
             * @brief Reads one MIDI 1 byte from BLE-MIDI packets after stripping timestamp bytes.
             *
             * @return Decoded MIDI 1 byte, or `std::nullopt` when no byte is available.
             */
            std::optional<uint8_t> read_midi_byte();
        } _transport;

        Hwa& _hwa;
    };
}    // namespace zlibs::utils::midi::ble
