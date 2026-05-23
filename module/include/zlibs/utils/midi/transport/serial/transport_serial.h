/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "transport_common.h"
#include "zlibs/utils/midi/midi.h"
#include "zlibs/utils/midi/transport/null/transport_null.h"

namespace zlibs::utils::midi::serial
{
#ifdef CONFIG_ZLIBS_UTILS_MIDI_TRANSPORT_SERIAL
    /**
     * @brief Serial transport implementation of the MIDI base engine.
     */
    class Serial : public virtual Base
    {
        public:
        /**
         * @brief Constructs a serial MIDI interface backed by a byte-stream hardware adapter.
         *
         * @param hwa Hardware abstraction used to send and receive serial MIDI bytes.
         */
        explicit Serial(Hwa& hwa)
            : _transport(*this)
            , _hwa(hwa)
        {
            bind_transport(_transport);
        }

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

            bool                    supported() override;
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
#else
    using Serial = midi::Null;
#endif
}    // namespace zlibs::utils::midi::serial
