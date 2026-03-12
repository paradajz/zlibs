/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "transport_common.h"
#include "zlibs/utils/midi/midi.h"

namespace zlibs::utils::midi::ble
{
    /**
     * @brief BLE transport implementation of the MIDI base engine.
     */
    class Ble : public Base
    {
        public:
        explicit Ble(Hwa& hwa)
            : Base(_transport)
            , _transport(*this)
            , _hwa(hwa)
        {}

        private:
        /**
         * @brief Internal BLE packetized transport.
         */
        class Transport : public midi::Transport
        {
            public:
            explicit Transport(Ble& ble)
                : _ble(ble)
            {}

            bool                   init() override;
            bool                   deinit() override;
            bool                   begin_transmission(MessageType type) override;
            bool                   write(uint8_t data) override;
            bool                   end_transmission() override;
            std::optional<uint8_t> read() override;

            private:
            Ble&                                                             _ble;
            Packet                                                           _tx_buffer      = {};
            size_t                                                           _rx_index       = 0;
            size_t                                                           _retrieve_index = 0;
            std::array<uint8_t, CONFIG_ZLIBS_UTILS_MIDI_BLE_MAX_PACKET_SIZE> _rx_buffer      = {};
            uint8_t                                                          _low_timestamp  = 0;
        } _transport;

        Hwa& _hwa;
    };
}    // namespace zlibs::utils::midi::ble
