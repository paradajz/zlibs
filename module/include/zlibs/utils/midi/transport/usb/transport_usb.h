/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "transport_common.h"
#include "zlibs/utils/midi/midi.h"
#include "zlibs/utils/midi/transport/null/transport_null.h"

namespace zlibs::utils::midi::usb
{
#ifdef CONFIG_ZLIBS_UTILS_MIDI_TRANSPORT_USB
    /**
     * @brief USB transport implementation of the MIDI base engine.
     */
    class Usb : public virtual Base
    {
        public:
        /**
         * @brief Constructs a USB MIDI interface backed by a UMP-capable hardware adapter.
         *
         * @param hwa Hardware abstraction used to send and receive USB MIDI UMP packets.
         */
        explicit Usb(Hwa& hwa)
            : _transport(*this)
            , _hwa(hwa)
        {
            bind_transport(_transport);
        }

        private:
        /**
         * @brief Internal USB-MIDI UMP transport.
         */
        class Transport : public midi::Transport
        {
            public:
            /**
             * @brief Constructs a USB-MIDI transport bridge.
             *
             * @param usb Owning USB MIDI instance.
             */
            explicit Transport(Usb& usb)
                : _usb(usb)
            {}

            bool                    supported() override;
            bool                    init() override;
            bool                    deinit() override;
            bool                    write(const midi_ump& packet) override;
            std::optional<midi_ump> read() override;

            private:
            Usb& _usb;
        } _transport;

        Hwa& _hwa;
    };
#else
    using Usb = midi::Null;
#endif
}    // namespace zlibs::utils::midi::usb
