/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "transport_common.h"
#include "zlibs/utils/midi/midi.h"

namespace zlibs::utils::midi::serial
{
    /**
     * @brief Serial transport implementation of the MIDI base engine.
     */
    class Serial : public Base
    {
        public:
        /**
         * @brief Constructs a serial MIDI interface backed by a byte-stream hardware adapter.
         *
         * @param hwa Hardware abstraction used to send and receive serial MIDI bytes.
         */
        explicit Serial(Hwa& hwa)
            : Base(_transport)
            , _transport(*this)
            , _hwa(hwa)
        {}

        private:
        /**
         * @brief Internal byte-stream serial transport.
         */
        class Transport : public midi::Transport
        {
            public:
            /**
             * @brief Constructs a serial MIDI transport bridge.
             *
             * @param serial Owning serial MIDI instance.
             */
            explicit Transport(Serial& serial)
                : _serial(serial)
            {}

            bool                    init() override;
            bool                    deinit() override;
            bool                    write(const midi_ump& packet) override;
            std::optional<midi_ump> read() override;

            private:
            Serial&              _serial;
            Midi1ByteToUmpParser _parser = {};

            /**
             * @brief Writes one MIDI 1 wire byte through the serial hardware adapter.
             *
             * @param data MIDI 1 byte to transmit.
             *
             * @return true when the byte was accepted by the hardware adapter.
             */
            bool write_midi_byte(uint8_t data);
        } _transport;

        Hwa& _hwa;
    };
}    // namespace zlibs::utils::midi::serial
