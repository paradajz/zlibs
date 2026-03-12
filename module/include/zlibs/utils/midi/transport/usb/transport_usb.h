/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "transport_common.h"
#include "zlibs/utils/midi/midi.h"

namespace zlibs::utils::midi::usb
{
    /**
     * @brief USB transport implementation of the MIDI base engine.
     */
    class Usb : public Base
    {
        public:
        explicit Usb(Hwa& hwa, uint8_t cin = 0)
            : Base(_transport)
            , _transport(*this, cin)
            , _hwa(hwa)
        {}

        private:
        /**
         * @brief Internal USB packetized transport.
         */
        class Transport : public midi::Transport
        {
            public:
            explicit Transport(Usb& usb, uint8_t cin)
                : _usb(usb)
                , CABLE_INDEX(cin)
            {}

            bool                   init() override;
            bool                   deinit() override;
            bool                   begin_transmission(MessageType type) override;
            bool                   write(uint8_t data) override;
            bool                   end_transmission() override;
            std::optional<uint8_t> read() override;

            private:
            /**
             * @brief USB-specific event nibbles for system and SysEx messages.
             */
            enum class SystemEvent : uint8_t
            {
                SysCommon1Byte = 0x50,
                SysCommon2Byte = 0x20,
                SysCommon3Byte = 0x30,
                SingleByte     = 0xF0,
                SysExStart     = 0x40,
                SysExStop1Byte = 0x50,
                SysExStop2Byte = 0x60,
                SysExStop3Byte = 0x70
            };

            Usb&          _usb;
            const uint8_t CABLE_INDEX;
            uint8_t       _rx_index     = 0;
            uint8_t       _rx_buffer[3] = {};
            Packet        _tx_buffer    = {};
            uint8_t       _tx_index     = 0;
            MessageType   _active_type  = MessageType::Invalid;

            /**
             * @brief Creates USB-MIDI header nibble from cable and event.
             *
             * @param virtual_cable Virtual cable number.
             * @param event MIDI event/status byte.
             *
             * @return USB-MIDI packet header byte.
             */
            static constexpr uint8_t usb_midi_header(uint8_t virtual_cable, uint8_t event)
            {
                return ((virtual_cable << 4) | (event >> 4));
            }
        } _transport;

        Hwa& _hwa;
    };
}    // namespace zlibs::utils::midi::usb
