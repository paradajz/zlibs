/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/midi/transport/transport_base.h"

namespace zlibs::utils::midi::usb
{
    /**
     * @brief Hardware abstraction interface for USB-MIDI transport.
     */
    class Hwa : public midi::Transport
    {
        public:
        ~Hwa() override = default;
    };
}    // namespace zlibs::utils::midi::usb
