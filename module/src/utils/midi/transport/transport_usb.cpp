/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/midi/transport/usb/transport_usb.h"

using namespace zlibs::utils::midi::usb;

bool Usb::Transport::supported()
{
    return _usb._hwa.supported();
}

bool Usb::Transport::init()
{
    return _usb._hwa.init();
}

bool Usb::Transport::deinit()
{
    return _usb._hwa.deinit();
}

bool Usb::Transport::write(const midi_ump& packet)
{
    return _usb._hwa.write(packet);
}

std::optional<midi_ump> Usb::Transport::read()
{
    return _usb._hwa.read();
}
