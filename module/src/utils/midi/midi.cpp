/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/midi/midi.h"

using namespace zlibs::utils::midi;

bool Base::init()
{
    if (_initialized)
    {
        return true;
    }

    reset();

    if (_transport.init())
    {
        _initialized = true;
        return true;
    }

    return false;
}

bool Base::deinit()
{
    if (!_initialized)
    {
        return true;
    }

    reset();
    _initialized = false;
    return _transport.deinit();
}

bool Base::initialized()
{
    return _initialized;
}

void Base::reset()
{
    _running_status_rx               = 0;
    _pending_message_expected_length = 0;
    _pending_message_index           = 0;
}

Transport& Base::transport()
{
    return _transport;
}

bool Base::send(MessageType type, uint8_t data1, uint8_t data2, uint8_t channel)
{
    const zlibs::utils::misc::LockGuard LOCK(_tx_mutex);
    bool                                channel_valid = true;

    // test if channel is valid
    if ((channel > MIDI_CHANNEL_MAX) || (channel < MIDI_CHANNEL_MIN))
    {
        channel_valid = false;
    }

    if (!channel_valid || (type < MessageType::NoteOff))
    {
        if (_use_running_status)
        {
            _running_status_tx = static_cast<uint8_t>(MessageType::Invalid);
        }

        return false;    // don't send anything
    }

    if (type <= MessageType::PitchBend)
    {
        // channel messages
        // protection: remove MSBs on data
        data1 &= MAX_VALUE_7BIT;
        data2 &= MAX_VALUE_7BIT;

        const uint8_t STATUS = status(type, channel);

        if (_transport.begin_transmission(type))
        {
            if (_use_running_status)
            {
                if (_running_status_tx != STATUS)
                {
                    // new message, memorize and send header
                    _running_status_tx = STATUS;

                    if (!_transport.write(_running_status_tx))
                    {
                        return false;
                    }
                }
            }
            else
            {
                // don't care about running status, send the status byte
                if (!_transport.write(STATUS))
                {
                    return false;
                }
            }

            // send data
            if (!_transport.write(data1))
            {
                return false;
            }

            if ((type != MessageType::ProgramChange) && (type != MessageType::AfterTouchChannel))
            {
                if (!_transport.write(data2))
                {
                    return false;
                }
            }

            return _transport.end_transmission();
        }
    }
    else if ((type >= MessageType::SysCommonTuneRequest) && (type <= MessageType::SysRealTimeSystemReset))
    {
        // system real-time and 1 byte
        return send_real_time(type);
    }

    return false;
}

bool Base::send_note_on(uint8_t note_number, uint8_t velocity, uint8_t channel)
{
    return send(MessageType::NoteOn, note_number, velocity, channel);
}

bool Base::send_note_off(uint8_t note_number, uint8_t velocity, uint8_t channel)
{
    if (_note_off_mode == NoteOffType::StandardNoteOff)
    {
        return send(MessageType::NoteOff, note_number, velocity, channel);
    }

    return send(MessageType::NoteOn, note_number, velocity, channel);
}

bool Base::send_program_change(uint8_t program_number, uint8_t channel)
{
    return send(MessageType::ProgramChange, program_number, 0, channel);
}

bool Base::send_control_change(uint8_t control_number, uint8_t control_value, uint8_t channel)
{
    return send(MessageType::ControlChange, control_number, control_value, channel);
}

bool Base::send_after_touch(uint8_t pressure, uint8_t channel, uint8_t note_number)
{
    return send(MessageType::AfterTouchPoly, note_number, pressure, channel);
}

bool Base::send_after_touch(uint8_t pressure, uint8_t channel)
{
    return send(MessageType::AfterTouchChannel, pressure, 0, channel);
}

bool Base::send_pitch_bend(uint16_t pitch_value, uint8_t channel)
{
    auto split = Split14Bit(pitch_value & MAX_VALUE_14BIT);

    return send(MessageType::PitchBend, split.low(), split.high(), channel);
}

bool Base::send_sys_ex(std::span<const uint8_t> data, bool array_contains_boundaries)
{
    const zlibs::utils::misc::LockGuard LOCK(_tx_mutex);

    if (_transport.begin_transmission(MessageType::SysEx))
    {
        if (!array_contains_boundaries)
        {
            if (!_transport.write(static_cast<uint8_t>(MessageType::SysEx)))
            {
                return false;
            }
        }

        for (const auto& byte : data)
        {
            if (!_transport.write(byte))
            {
                return false;
            }
        }

        if (!array_contains_boundaries)
        {
            if (!_transport.write(SYS_EX_END))
            {
                return false;
            }
        }

        if (_transport.end_transmission())
        {
            if (_use_running_status)
            {
                _running_status_tx = static_cast<uint8_t>(MessageType::Invalid);
            }

            return true;
        }
    }

    return false;
}

bool Base::send_tune_request()
{
    return send_common(MessageType::SysCommonTuneRequest, 0);
}

bool Base::send_time_code_quarter_frame(uint8_t type_nibble, uint8_t values_nibble)
{
    const uint8_t DATA_BYTE = (((type_nibble & MTC_TYPE_MASK) << 4) | (values_nibble & MTC_DATA_MASK));
    return send_common(MessageType::SysCommonTimeCodeQuarterFrame, DATA_BYTE);
}

bool Base::send_time_code_quarter_frame(uint8_t data)
{
    return send_common(MessageType::SysCommonTimeCodeQuarterFrame, data);
}

bool Base::send_song_position(uint16_t beats)
{
    return send_common(MessageType::SysCommonSongPosition, beats);
}

bool Base::send_song_select(uint8_t song_number)
{
    return send_common(MessageType::SysCommonSongSelect, song_number);
}

bool Base::send_common(MessageType type, uint8_t data1)
{
    const zlibs::utils::misc::LockGuard LOCK(_tx_mutex);

    switch (type)
    {
    case MessageType::SysCommonTimeCodeQuarterFrame:
    case MessageType::SysCommonSongPosition:
    case MessageType::SysCommonSongSelect:
    case MessageType::SysCommonTuneRequest:
        break;    // valid

    default:
        return false;
    }

    if (_transport.begin_transmission(type))
    {
        if (!_transport.write(static_cast<uint8_t>(type)))
        {
            return false;
        }

        switch (type)
        {
        case MessageType::SysCommonTimeCodeQuarterFrame:
        {
            if (!_transport.write(data1))
            {
                return false;
            }
        }
        break;

        case MessageType::SysCommonSongPosition:
        {
            if (!_transport.write(data1 & MAX_VALUE_7BIT))
            {
                return false;
            }

            if (!_transport.write((data1 >> DATA_MSB_SHIFT) & MAX_VALUE_7BIT))
            {
                return false;
            }
        }
        break;

        case MessageType::SysCommonSongSelect:
        {
            if (!_transport.write(data1 & MAX_VALUE_7BIT))
            {
                return false;
            }
        }
        break;

        default:
            break;
        }

        if (_transport.end_transmission())
        {
            if (_use_running_status)
            {
                _running_status_tx = static_cast<uint8_t>(MessageType::Invalid);
            }
        }
    }

    return false;
}

bool Base::send_real_time(MessageType type)
{
    const zlibs::utils::misc::LockGuard LOCK(_tx_mutex);

    switch (type)
    {
    case MessageType::SysRealTimeClock:
    case MessageType::SysRealTimeStart:
    case MessageType::SysRealTimeStop:
    case MessageType::SysRealTimeContinue:
    case MessageType::SysRealTimeActiveSensing:
    case MessageType::SysRealTimeSystemReset:
    {
        if (_transport.begin_transmission(type))
        {
            if (!_transport.write(static_cast<uint8_t>(type)))
            {
                return false;
            }

            return _transport.end_transmission();
        }
    }
    break;

    default:
        return false;
    }

    return false;
}

bool Base::send_mmc(uint8_t device_id, MessageType mmc)
{
    switch (mmc)
    {
    case MessageType::MmcPlay:
    case MessageType::MmcStop:
    case MessageType::MmcPause:
    case MessageType::MmcRecordStart:
    case MessageType::MmcRecordStop:
        break;

    default:
        return false;
    }

    uint8_t mmc_array[MMC_MESSAGE_LENGTH] = {
        static_cast<uint8_t>(MessageType::SysEx),
        MMC_ALL_CALL_DEVICE_ID,
        MMC_ALL_CALL_DEVICE_ID,
        MMC_SUB_ID,
        MMC_COMMAND_DEFAULT,
        SYS_EX_END
    };
    mmc_array[2] = device_id;
    mmc_array[4] = static_cast<uint8_t>(mmc);

    return send_sys_ex(mmc_array, true);
}

bool Base::send_nrpn(uint16_t parameter_number, uint16_t value, uint8_t channel, bool value_14bit)
{
    auto parameter_number_split = Split14Bit(parameter_number);

    if (!send_control_change(NRPN_MSB_CONTROLLER, parameter_number_split.high(), channel))
    {
        return false;
    }

    if (!send_control_change(NRPN_LSB_CONTROLLER, parameter_number_split.low(), channel))
    {
        return false;
    }

    if (!value_14bit)
    {
        return send_control_change(DATA_ENTRY_MSB_CONTROLLER, value, channel);
    }

    auto value_split = Split14Bit(value);

    if (!send_control_change(DATA_ENTRY_MSB_CONTROLLER, value_split.high(), channel))
    {
        return false;
    }

    return send_control_change(DATA_ENTRY_LSB_CONTROLLER, value_split.low(), channel);
}

bool Base::send_control_change_14bit(uint16_t control_number, uint16_t control_value, uint8_t channel)
{
    auto control_value_split = Split14Bit(control_value);

    if (!send_control_change(control_number, control_value_split.high(), channel))
    {
        return false;
    }

    return send_control_change(control_number + CONTROL_CHANGE_LSB_OFFSET, control_value_split.low(), channel);
}

void Base::set_running_status_state(bool state)
{
    const zlibs::utils::misc::LockGuard LOCK(_tx_mutex);
    _use_running_status = state;
}

bool Base::running_status_state()
{
    const zlibs::utils::misc::LockGuard LOCK(_tx_mutex);
    return _use_running_status;
}

uint8_t Base::status(MessageType type, uint8_t channel)
{
    return (static_cast<uint8_t>(type) | ((channel - MIDI_CHANNEL_MIN) & MIDI_STATUS_NIBBLE_MASK));
}

bool Base::read()
{
    if (!parse())
    {
        return false;
    }

    thru();

    return true;
}

bool Base::parse()
{
    auto data = _transport.read();

    if (!data.has_value())
    {
        return false;    // no data available
    }

    const uint8_t EXTRACTED = data.value();

    if (_pending_message_index == 0)
    {
        // start a new pending message
        _pending_message[0] = EXTRACTED;

        // check for running status first (din only)
        if (is_channel_message(type_from_status_byte(_running_status_rx)))
        {
            // Only channel messages allow Running Status.
            // If the status byte is not received, prepend it to the pending message.
            if (EXTRACTED < MIDI_STATUS_MIN)
            {
                _pending_message[0]    = _running_status_rx;
                _pending_message[1]    = EXTRACTED;
                _pending_message_index = 1;
            }

            // Else: well, we received another status byte,
            // so the running status does not apply here.
            // It will be updated upon completion of this message.
        }

        const auto PENDING_TYPE = type_from_status_byte(_pending_message[0]);

        switch (PENDING_TYPE)
        {
        // 1 byte messages
        case MessageType::SysRealTimeStart:
        case MessageType::SysRealTimeContinue:
        case MessageType::SysRealTimeStop:
        case MessageType::SysRealTimeClock:
        case MessageType::SysRealTimeActiveSensing:
        case MessageType::SysRealTimeSystemReset:
        case MessageType::SysCommonTuneRequest:
        {
            // handle the message type directly here
            _message.type    = PENDING_TYPE;
            _message.channel = 0;
            _message.data1   = 0;
            _message.data2   = 0;
            _message.valid   = true;

            // do not reset all input attributes: running status must remain unchanged
            // we still need to reset these
            _pending_message_index           = 0;
            _pending_message_expected_length = 0;

            return true;
        }
        break;

        // 2 bytes messages
        case MessageType::ProgramChange:
        case MessageType::AfterTouchChannel:
        case MessageType::SysCommonTimeCodeQuarterFrame:
        case MessageType::SysCommonSongSelect:
        {
            _pending_message_expected_length = 2;
        }
        break;

        // 3 bytes messages
        case MessageType::NoteOn:
        case MessageType::NoteOff:
        case MessageType::ControlChange:
        case MessageType::PitchBend:
        case MessageType::AfterTouchPoly:
        case MessageType::SysCommonSongPosition:
        {
            _pending_message_expected_length = 3;
        }
        break;

        case MessageType::SysEx:
        {
            // the message can be any length between 3 and CONFIG_ZLIBS_UTILS_MIDI_SYSEX_ARRAY_SIZE
            _pending_message_expected_length = CONFIG_ZLIBS_UTILS_MIDI_SYSEX_ARRAY_SIZE;
            _running_status_rx               = static_cast<uint8_t>(MessageType::Invalid);
            _message.sys_ex_array[0]         = static_cast<uint8_t>(MessageType::SysEx);
        }
        break;

        case MessageType::Invalid:
        default:
        {
            reset();

            return false;
        }
        break;
        }

        if (_pending_message_index >= (_pending_message_expected_length - 1))
        {
            // reception complete
            _message.type                    = PENDING_TYPE;
            _message.channel                 = channel_from_status_byte(_pending_message[0]);
            _message.data1                   = _pending_message[1];
            _message.data2                   = 0;
            _message.length                  = 1;
            _pending_message_index           = 0;
            _pending_message_expected_length = 0;
            _message.valid                   = true;

            return true;
        }

        // waiting for more data
        _pending_message_index++;

        if (!_recursive_parse_state)
        {
            // message is not complete
            return false;
        }

        return parse();
    }

    // first, test if this is a status byte
    if (EXTRACTED >= MIDI_STATUS_MIN)
    {
        // Reception of status bytes in the middle of an uncompleted message
        // are allowed only for interleaved Real Time message or EOX.
        switch (EXTRACTED)
        {
        case static_cast<uint8_t>(MessageType::SysRealTimeClock):
        case static_cast<uint8_t>(MessageType::SysRealTimeStart):
        case static_cast<uint8_t>(MessageType::SysRealTimeContinue):
        case static_cast<uint8_t>(MessageType::SysRealTimeStop):
        case static_cast<uint8_t>(MessageType::SysRealTimeActiveSensing):
        case static_cast<uint8_t>(MessageType::SysRealTimeSystemReset):
        {
            // Here we will have to extract the one-byte message,
            // pass it to the structure for being read outside
            // the MIDI class, and recompose the message it was
            // interleaved into without killing the running status..
            // This is done by leaving the pending message as is,
            // it will be completed on next calls.
            _message.type    = static_cast<MessageType>(EXTRACTED);
            _message.data1   = 0;
            _message.data2   = 0;
            _message.channel = 0;
            _message.length  = 1;
            _message.valid   = true;

            return true;
        }
        break;

        // end of sysex
        case SYS_EX_END:
        {
            if (_message.sys_ex_array[0] == static_cast<uint8_t>(MessageType::SysEx))
            {
                // store the last byte (EOX)
                _message.sys_ex_array[_pending_message_index++] = SYS_EX_END;
                _message.type                                   = MessageType::SysEx;

                // get length
                _message.data1   = 0;
                _message.data2   = 0;
                _message.channel = 0;
                _message.length  = _pending_message_index;
                _message.valid   = true;

                reset();
                return true;
            }

            // error
            reset();
            return false;
        }
        break;

        // start of sysex
        case static_cast<uint8_t>(MessageType::SysEx):
        {
            // reset the parsing of sysex
            _message.sys_ex_array[0] = static_cast<uint8_t>(MessageType::SysEx);
            _pending_message_index   = 1;
        }
        break;

        default:
            break;
        }
    }

    // add extracted data byte to pending message
    if (_pending_message[0] == static_cast<uint8_t>(MessageType::SysEx))
    {
        _message.sys_ex_array[_pending_message_index] = EXTRACTED;
    }
    else
    {
        _pending_message[_pending_message_index] = EXTRACTED;
    }

    // now we are going to check if we have reached the end of the message
    if (_pending_message_index >= (_pending_message_expected_length - 1))
    {
        //"FML" case: fall down here with an overflown SysEx..
        // This means we received the last possible data byte that can fit the buffer.
        // If this happens, try increasing CONFIG_ZLIBS_UTILS_MIDI_SYSEX_ARRAY_SIZE.
        if (_pending_message[0] == static_cast<uint8_t>(MessageType::SysEx))
        {
            reset();
            return false;
        }

        _message.type = type_from_status_byte(_pending_message[0]);

        if (is_channel_message(_message.type))
        {
            _message.channel = channel_from_status_byte(_pending_message[0]);
        }
        else
        {
            _message.channel = 0;
        }

        _message.data1 = _pending_message[1];

        // save data2 only if applicable
        if (_pending_message_expected_length == 3)
        {
            _message.data2 = _pending_message[2];
        }
        else
        {
            _message.data2 = 0;
        }

        _message.length = _pending_message_expected_length;

        // reset local variables
        _pending_message_index           = 0;
        _pending_message_expected_length = 0;
        _message.valid                   = true;

        // activate running status (if enabled for the received type)
        switch (_message.type)
        {
        case MessageType::NoteOff:
        case MessageType::NoteOn:
        case MessageType::AfterTouchPoly:
        case MessageType::ControlChange:
        case MessageType::ProgramChange:
        case MessageType::AfterTouchChannel:
        case MessageType::PitchBend:
        {
            // running status enabled: store it from received message
            _running_status_rx = _pending_message[0];
        }
        break;

        default:
        {
            // no running status
            _running_status_rx = static_cast<uint8_t>(MessageType::Invalid);
        }
        break;
        }

        return true;
    }

    // update the index of the pending message
    _pending_message_index++;

    if (!_recursive_parse_state)
    {
        return false;    // message is not complete.
    }

    return parse();
}

MessageType Base::type()
{
    return _message.type;
}

uint8_t Base::channel()
{
    return _message.channel;
}

uint8_t Base::data1()
{
    return _message.data1;
}

uint8_t Base::data2()
{
    return _message.data2;
}

std::span<uint8_t> Base::sys_ex_array()
{
    return std::span<uint8_t>(_message.sys_ex_array, _message.length);
}

uint16_t Base::length()
{
    return _message.length;
}

void Base::use_recursive_parsing(bool state)
{
    _recursive_parse_state = state;
}

void Base::thru()
{
    for (size_t i = 0; i < _thru_interfaces.size(); i++)
    {
        auto interface = _thru_interfaces.at(i);

        if (interface == nullptr)
        {
            continue;
        }

        if (interface->begin_transmission(_message.type))
        {
            if (is_system_real_time(_message.type))
            {
                interface->write(static_cast<uint8_t>(_message.type));
            }
            else if (is_channel_message(_message.type))
            {
                const auto STATUS = status(_message.type, _message.channel);
                interface->write(static_cast<uint8_t>(STATUS));

                if (_message.length > 1)
                {
                    interface->write(_message.data1);
                }

                if (_message.length > 2)
                {
                    interface->write(_message.data2);
                }
            }
            else if (_message.type == MessageType::SysEx)
            {
                const auto SIZE = _message.length;

                for (size_t i = 0; i < SIZE; i++)
                {
                    interface->write(_message.sys_ex_array[i]);
                }
            }
            else    // at this point, it is assumed to be a system common message
            {
                interface->write(static_cast<uint8_t>(_message.type));

                if (_message.length > 1)
                {
                    interface->write(_message.data1);
                }

                if (_message.length > 2)
                {
                    interface->write(_message.data2);
                }
            }
        }

        interface->end_transmission();
    }
}

void Base::set_note_off_mode(NoteOffType type)
{
    _note_off_mode = type;
}

NoteOffType Base::note_off_mode()
{
    return _note_off_mode;
}

void Base::register_thru_interface(Thru& interface_ref)
{
    for (size_t i = 0; i < _thru_interfaces.size(); i++)
    {
        if (_thru_interfaces.at(i) == nullptr)
        {
            _thru_interfaces[i] = &interface_ref;
            break;
        }
    }
}

void Base::unregister_thru_interface(Thru& interface_ref)
{
    for (size_t i = 0; i < _thru_interfaces.size(); i++)
    {
        if (_thru_interfaces.at(i) == &interface_ref)
        {
            _thru_interfaces[i] = nullptr;
        }
    }
}

// return the last decoded midi message
Message& Base::message()
{
    return _message;
}
