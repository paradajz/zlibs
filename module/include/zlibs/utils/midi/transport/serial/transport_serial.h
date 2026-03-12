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
            explicit Transport(Serial& serial)
                : _serial(serial)
            {}

            bool                   init() override;
            bool                   deinit() override;
            bool                   begin_transmission(MessageType type) override;
            bool                   write(uint8_t data) override;
            bool                   end_transmission() override;
            std::optional<uint8_t> read() override;

            private:
            Serial& _serial;
        } _transport;

        Hwa& _hwa;
    };
}    // namespace zlibs::utils::midi::serial
