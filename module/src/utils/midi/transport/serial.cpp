/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/midi/transport/serial/transport_serial.h"

using namespace zlibs::utils::midi::serial;

bool Serial::Transport::init()
{
    _serial.use_recursive_parsing(true);
    return _serial._hwa.init();
}

bool Serial::Transport::deinit()
{
    return _serial._hwa.deinit();
}

bool Serial::Transport::begin_transmission([[maybe_unused]] MessageType type)
{
    // nothing to do
    return true;
}

bool Serial::Transport::write(uint8_t data)
{
    Packet packet = { data };

    return _serial._hwa.write(packet);
}

bool Serial::Transport::end_transmission()
{
    // nothing to do
    return true;
}

std::optional<uint8_t> Serial::Transport::read()
{
    auto packet = _serial._hwa.read();

    if (!packet.has_value())
    {
        return {};
    }

    return packet.value().data;
}
