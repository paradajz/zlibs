/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/midi/transport/serial/transport_serial.h"

using namespace zlibs::utils::midi::serial;

bool Serial::Transport::init()
{
    return _serial._hwa.init();
}

bool Serial::Transport::deinit()
{
    return _serial._hwa.deinit();
}

bool Serial::Transport::write_midi_byte(uint8_t data)
{
    return _serial._hwa.write(data);
}

bool Serial::Transport::write(const midi_ump& packet)
{
    return midi::write_ump_as_midi1_bytes(packet,
                                          UMP_GROUP(packet),
                                          [this](uint8_t data)
                                          {
                                              return write_midi_byte(data);
                                          });
}

std::optional<midi_ump> Serial::Transport::read()
{
    return midi::read_midi1_bytes_as_ump(_parser,
                                         DEFAULT_RX_GROUP,
                                         [this]()
                                         {
                                             return _serial._hwa.read();
                                         });
}
