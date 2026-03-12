/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/midi/transport/usb/transport_usb.h"

using namespace zlibs::utils::midi::usb;

bool Usb::Transport::init()
{
    _tx_index = 0;
    _rx_index = 0;
    _usb.use_recursive_parsing(true);

    return _usb._hwa.init();
}

bool Usb::Transport::deinit()
{
    return _usb._hwa.deinit();
}

bool Usb::Transport::begin_transmission(MessageType type)
{
    _active_type                   = type;
    _tx_buffer.data[Packet::Event] = usb_midi_header(CABLE_INDEX, static_cast<uint8_t>(type));
    _tx_index                      = 0;

    return true;
}

bool Usb::Transport::write(uint8_t data)
{
    bool return_value = true;

    if (_active_type != MessageType::SysEx)
    {
        _tx_buffer.data[_tx_index + 1] = data;
    }
    else if (data == static_cast<uint8_t>(MessageType::SysEx))
    {
        // start of sysex
        _tx_buffer.data[Packet::Event] = usb_midi_header(CABLE_INDEX, static_cast<uint8_t>(SystemEvent::SysExStart));
        _tx_buffer.data[Packet::Data1] = data;
    }
    else
    {
        auto i = _tx_index % 3;

        if (data == SYS_EX_END)
        {
            // End of sysex:
            // this event has sys_ex_stop_1_byte as event index with added count of how many bytes there are in USB packet.
            // Add 0x10 since event is shifted 4 bytes to the left.

            _tx_buffer.data[Packet::Event] = usb_midi_header(CABLE_INDEX,
                                                             (static_cast<uint8_t>(SystemEvent::SysExStop1Byte) +
                                                              (SYS_EX_USB_EVENT_STEP * i)));
        }

        switch (i)
        {
        case 0:
        {
            _tx_buffer.data[Packet::Data1] = data;
            _tx_buffer.data[Packet::Data2] = 0;
            _tx_buffer.data[Packet::Data3] = 0;
        }
        break;

        case 1:
        {
            _tx_buffer.data[Packet::Data2] = data;
            _tx_buffer.data[Packet::Data3] = 0;
        }
        break;

        case 2:
        {
            _tx_buffer.data[Packet::Data3] = data;

            if (data != SYS_EX_END)
            {
                return_value = end_transmission();
            }
        }
        break;

        default:
            break;
        }
    }

    _tx_index++;

    return return_value;
}

bool Usb::Transport::end_transmission()
{
    return _usb._hwa.write(_tx_buffer);
}

std::optional<uint8_t> Usb::Transport::read()
{
    if (!_rx_index)
    {
        auto packet = _usb._hwa.read();

        if (!packet.has_value())
        {
            return {};
        }

        // We already have the entire message here.
        // Packet Event nibble is CIN, see the USB-MIDI specification.
        // Shift CIN nibble to the high half-byte to match MessageType encoding.
        uint8_t midi_message = packet.value().data[Packet::Event] << 4;

        switch (midi_message)
        {
        // 1 byte messages
        case static_cast<uint8_t>(SystemEvent::SysCommon1Byte):
        case static_cast<uint8_t>(SystemEvent::SingleByte):
        {
            _rx_buffer[_rx_index++] = packet.value().data[Packet::Data1];
        }
        break;

        // 2 byte messages
        case static_cast<uint8_t>(SystemEvent::SysCommon2Byte):
        case static_cast<uint8_t>(MessageType::ProgramChange):
        case static_cast<uint8_t>(MessageType::AfterTouchChannel):
        case static_cast<uint8_t>(MessageType::SysCommonTimeCodeQuarterFrame):
        case static_cast<uint8_t>(MessageType::SysCommonSongSelect):
        case static_cast<uint8_t>(SystemEvent::SysExStop2Byte):
        {
            _rx_buffer[_rx_index++] = packet.value().data[Packet::Data2];
            _rx_buffer[_rx_index++] = packet.value().data[Packet::Data1];
        }
        break;

        // 3 byte messages
        case static_cast<uint8_t>(MessageType::NoteOn):
        case static_cast<uint8_t>(MessageType::NoteOff):
        case static_cast<uint8_t>(MessageType::ControlChange):
        case static_cast<uint8_t>(MessageType::PitchBend):
        case static_cast<uint8_t>(MessageType::AfterTouchPoly):
        case static_cast<uint8_t>(MessageType::SysCommonSongPosition):
        case static_cast<uint8_t>(SystemEvent::SysExStart):
        case static_cast<uint8_t>(SystemEvent::SysExStop3Byte):
        {
            _rx_buffer[_rx_index++] = packet.value().data[Packet::Data3];
            _rx_buffer[_rx_index++] = packet.value().data[Packet::Data2];
            _rx_buffer[_rx_index++] = packet.value().data[Packet::Data1];
        }
        break;

        default:
            return {};
        }
    }

    if (_rx_index)
    {
        const auto DATA = _rx_buffer[_rx_index - 1];
        _rx_index--;

        return DATA;
    }

    return {};
}
