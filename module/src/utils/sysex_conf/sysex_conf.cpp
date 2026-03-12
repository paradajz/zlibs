/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/sysex_conf/sysex_conf.h"

using namespace zlibs::utils::sysex_conf;

void SysExConf::reset()
{
    _configuration_enabled          = false;
    _user_error_ignore_mode_enabled = false;
    _decoded_request                = {};
    _response_counter               = 0;
    _layout                         = {};
    _custom_requests                = {};

    reset_ump_message();
}

bool SysExConf::set_layout(std::span<const Block> layout)
{
    _configuration_enabled = false;

    if (!layout.empty())
    {
        _layout = layout;
        return true;
    }

    return false;
}

bool SysExConf::setup_custom_requests(std::span<const CustomRequest> custom_requests)
{
    if (!custom_requests.empty())
    {
        _custom_requests = custom_requests;

        for (std::size_t i = 0; i < _custom_requests.size(); i++)
        {
            if (_custom_requests[i].request_id < static_cast<uint8_t>(SpecialRequest::Count))
            {
                _custom_requests = {};
                return false;    // ID is already used internally.
            }
        }

        return true;
    }

    return false;
}

bool SysExConf::is_configuration_enabled()
{
    return _configuration_enabled;
}

void SysExConf::set_user_error_ignore_mode(bool state)
{
    _user_error_ignore_mode_enabled = state;
}

void SysExConf::handle_request(std::span<const uint8_t> message)
{
    if (_layout.empty())
    {
        return;
    }

    if (message.size() < SPECIAL_REQ_MSG_SIZE)
    {
        return;    // Ignore small messages.
    }

    if (message[0] != SYSEX_START_BYTE)
    {
        return;
    }

    if (message[message.size() - 1] != SYSEX_END_BYTE)
    {
        return;
    }

    if (message.size() > MAX_MESSAGE_SIZE)
    {
        return;
    }

    reset_decoded_request();

    // Copy the entire incoming message to the internal buffer.
    for (size_t i = 0; i < message.size(); i++)
    {
        _response_array[i] = message[i];
    }

    // Set the response counter to the last position in the request.
    _response_counter = message.size() - 1;

    if (!check_manufacturer_id())
    {
        return;    // Do not send a response to the wrong ID.
    }

    bool should_send_response = true;

    if (!check_status())
    {
        set_status(Status::ErrorStatus);
    }
    else
    {
        if (decode_standard_request(message))
        {
            if (message.size() == SPECIAL_REQ_MSG_SIZE)
            {
                process_special_request();
            }
            else
            {
                if (process_standard_request(message.size()))
                {
                    /*
                     * In this case, `process_standard_request()` calls
                     * `send_response()` internally, so there is no need to call
                     * it again here.
                     */
                    should_send_response = false;
                }
                else
                {
                    /*
                     * The handler returned an error, so send the response
                     * manually and reset the decoded request.
                     */
                    reset_decoded_request();
                }
            }
        }
        else
        {
            reset_decoded_request();
        }
    }

    if (should_send_response)
    {
        send_response();
    }
}

void SysExConf::handle_packet(const midi_ump& packet)
{
    if (!midi::is_sysex7_packet(packet))
    {
        reset_ump_message();
        return;
    }

    const auto    sysex_status = midi::sysex7_status(packet);
    const uint8_t size         = midi::sysex7_payload_size(packet);
    const bool    starts_frame = midi::sysex7_starts_message(sysex_status);
    const bool    ends_frame   = midi::sysex7_ends_message(sysex_status);

    if (starts_frame)
    {
        reset_ump_message();
        _response_array[_response_counter++] = SYSEX_START_BYTE;
        _ump_message_active                  = true;
    }
    else if (!_ump_message_active)
    {
        return;
    }

    const size_t required_size = static_cast<size_t>(_response_counter) + size + (ends_frame ? 1 : 0);

    if (required_size > MAX_MESSAGE_SIZE)
    {
        reset_ump_message();
        return;
    }

    for (size_t i = 0; i < size; i++)
    {
        _response_array[_response_counter++] = midi::read_sysex7_payload_byte(packet, i);
    }

    if (!ends_frame)
    {
        return;
    }

    _response_array[_response_counter++] = SYSEX_END_BYTE;
    _ump_message_active                  = false;
    handle_request(std::span<const uint8_t>(_response_array, _response_counter));
    reset_ump_message();
}

void SysExConf::reset_decoded_request()
{
    _decoded_request.status    = Status::Ack;
    _decoded_request.wish      = Wish::Invalid;
    _decoded_request.amount    = Amount::Invalid;
    _decoded_request.block     = 0;
    _decoded_request.section   = 0;
    _decoded_request.part      = 0;
    _decoded_request.index     = 0;
    _decoded_request.new_value = 0;
}

void SysExConf::reset_ump_message()
{
    _response_counter   = 0;
    _ump_message_active = false;
}

bool SysExConf::decode_standard_request(std::span<const uint8_t> message)
{
    if (message.size() == SPECIAL_REQ_MSG_SIZE)
    {
        // Special request.
        return true;    // Checked in `process_special_request()`.
    }

    if (message.size() < (STD_REQ_MIN_MSG_SIZE - BYTES_PER_VALUE))    // No index here.
    {
        set_status(Status::ErrorMessageLength);
        return false;
    }

    if (!_configuration_enabled)
    {
        // Connection-open request has not been received.
        set_status(Status::ErrorConnection);
        return false;
    }

    // Do not read these fields unless the message is long enough.
    _decoded_request.part    = message[static_cast<uint8_t>(ByteOrder::PartByte)];
    _decoded_request.wish    = static_cast<Wish>(message[static_cast<uint8_t>(ByteOrder::WishByte)]);
    _decoded_request.amount  = static_cast<Amount>(message[static_cast<uint8_t>(ByteOrder::AmountByte)]);
    _decoded_request.block   = message[static_cast<uint8_t>(ByteOrder::BlockByte)];
    _decoded_request.section = message[static_cast<uint8_t>(ByteOrder::SectionByte)];

    if (!check_wish())
    {
        set_status(Status::ErrorWish);
        return false;
    }

    if (!check_block())
    {
        set_status(Status::ErrorBlock);
        return false;
    }

    if (!check_section())
    {
        set_status(Status::ErrorSection);
        return false;
    }

    if (!check_amount())
    {
        set_status(Status::ErrorAmount);
        return false;
    }

    if (!check_part())
    {
        set_status(Status::ErrorPart);
        return false;
    }

    if (message.size() != generate_message_length())
    {
        set_status(Status::ErrorMessageLength);
        return false;
    }

    // Start building the response.
    set_status(Status::Ack);

    if (_decoded_request.amount == Amount::Single)
    {
        auto merged_index = Merge14Bit(
            message[static_cast<uint8_t>(ByteOrder::IndexByte)],
            message[static_cast<uint8_t>(ByteOrder::IndexByte) + 1]);
        _decoded_request.index = merged_index();

        if (_decoded_request.wish == Wish::Set)
        {
            auto merged_new_value =
                Merge14Bit(message[static_cast<uint8_t>(ByteOrder::IndexByte) + BYTES_PER_VALUE],
                           message[static_cast<uint8_t>(ByteOrder::IndexByte) + BYTES_PER_VALUE + 1]);
            _decoded_request.new_value = merged_new_value();
        }
    }

    return true;
}

bool SysExConf::process_standard_request(uint16_t received_array_size)
{
    uint16_t start_index = 0, end_index = 1;
    uint8_t  msg_parts_loop = 1, response_counter_local = _response_counter;
    bool     all_parts_ack  = false;
    bool     all_parts_loop = false;

    if ((_decoded_request.wish == Wish::Backup) || (_decoded_request.wish == Wish::Get))
    {
        if ((_decoded_request.part == PART_ALL_MESSAGES) || (_decoded_request.part == PART_ALL_MESSAGES_WITH_ACK))
        {
            /*
             * When these parts are specified, the protocol loops over all
             * message parts and returns one response per part.
             */
            msg_parts_loop = _layout[_decoded_request.block]
                                 ._sections[_decoded_request.section]
                                 .parts();
            all_parts_loop = true;

            // When part is set to 126 (0x7E), send final ACK once all parts are sent.
            if (_decoded_request.part == PART_ALL_MESSAGES_WITH_ACK)
            {
                all_parts_ack = true;
            }
        }

        if (_decoded_request.wish == Wish::Backup)
        {
            // Convert the response to a request.
            _response_array[static_cast<uint8_t>(ByteOrder::StatusByte)] = static_cast<uint8_t>(Status::Request);
            // Convert response wish to SET.
            _response_array[static_cast<uint8_t>(ByteOrder::WishByte)] = (uint8_t)Wish::Set;
            // Decode path needs GET while retrieving values.
            _decoded_request.wish = Wish::Get;
            // When backup is a request, remove the received index/new value from the response.
            response_counter_local = received_array_size - 1 - (2 * BYTES_PER_VALUE);
        }
    }

    for (int j = 0; j < msg_parts_loop; j++)
    {
        _response_counter = response_counter_local;

        if (all_parts_loop)
        {
            _decoded_request.part                                      = j;
            _response_array[static_cast<uint8_t>(ByteOrder::PartByte)] = j;
        }

        if (_decoded_request.amount == Amount::All)
        {
            start_index = PARAMS_PER_MESSAGE * _decoded_request.part;
            end_index   = start_index + PARAMS_PER_MESSAGE;

            if (end_index > _layout[_decoded_request.block]
                                ._sections[_decoded_request.section]
                                .number_of_parameters())
            {
                end_index = _layout[_decoded_request.block]
                                ._sections[_decoded_request.section]
                                .number_of_parameters();
            }
        }

        for (uint16_t i = start_index; i < end_index; i++)
        {
            switch (_decoded_request.wish)
            {
            case Wish::Get:
            {
                if (_decoded_request.amount == Amount::Single)
                {
                    if (!check_parameter_index())
                    {
                        set_status(Status::ErrorIndex);
                        return false;
                    }

                    uint16_t value  = 0;
                    Status   result = _data_handler.get(_decoded_request.block,
                                                        _decoded_request.section,
                                                        _decoded_request.index,
                                                        value);

                    switch (result)
                    {
                    case Status::Ack:
                    {
                        add_to_response(value);
                    }
                    break;

                    default:
                    {
                        if (_user_error_ignore_mode_enabled)
                        {
                            value = 0;
                            add_to_response(value);
                        }
                        else
                        {
                            set_status(result);
                            return false;
                        }
                    }
                    break;
                    }
                }
                else
                {
                    // Get all parameters; no index is specified.
                    uint16_t value  = 0;
                    Status   result = _data_handler.get(_decoded_request.block, _decoded_request.section, i, value);

                    switch (result)
                    {
                    case Status::Ack:
                    {
                        add_to_response(value);
                    }
                    break;

                    default:
                    {
                        if (_user_error_ignore_mode_enabled)
                        {
                            value = 0;
                            add_to_response(value);
                        }
                        else
                        {
                            set_status(result);
                            return false;
                        }
                    }
                    break;
                    }
                }
            }
            break;

            default:
            {
                // Wish::Set path.
                if (_decoded_request.amount == Amount::Single)
                {
                    if (!check_parameter_index())
                    {
                        set_status(Status::ErrorIndex);
                        return false;
                    }

                    if (!check_new_value())
                    {
                        set_status(Status::ErrorNewValue);
                        return false;
                    }

                    Status result = _data_handler.set(_decoded_request.block, _decoded_request.section, _decoded_request.index, _decoded_request.new_value);

                    switch (result)
                    {
                    case Status::Ack:
                        break;

                    default:
                    {
                        if (!_user_error_ignore_mode_enabled)
                        {
                            set_status(result);
                            return false;
                        }
                    }
                    break;
                    }
                }
                else
                {
                    uint8_t array_index = (i - start_index);

                    array_index *= BYTES_PER_VALUE;
                    array_index += static_cast<uint8_t>(ByteOrder::IndexByte);

                    auto merge                 = Merge14Bit(_response_array[array_index],
                                                            _response_array[array_index + 1]);
                    _decoded_request.new_value = merge();

                    if (!check_new_value())
                    {
                        set_status(Status::ErrorNewValue);
                        return false;
                    }

                    Status result = _data_handler.set(_decoded_request.block,
                                                      _decoded_request.section,
                                                      i,
                                                      _decoded_request.new_value);

                    switch (result)
                    {
                    case Status::Ack:
                        break;

                    default:
                    {
                        if (!_user_error_ignore_mode_enabled)
                        {
                            set_status(result);
                            return false;
                        }
                    }
                    break;
                    }
                }
            }
            break;
            }
        }

        send_response();
    }

    if (all_parts_ack)
    {
        // Send final ACK message at the end of all-parts transfer.
        _response_counter                    = 0;
        _response_array[_response_counter++] = SYSEX_START_BYTE;
        _response_array[_response_counter++] = _manufacturer_id.id1;
        _response_array[_response_counter++] = _manufacturer_id.id2;
        _response_array[_response_counter++] = _manufacturer_id.id3;
        _response_array[_response_counter++] = static_cast<uint8_t>(Status::Ack);
        _response_array[_response_counter++] = PART_ALL_MESSAGES_ACK_PART;
        _response_array[_response_counter++] = static_cast<uint8_t>(_decoded_request.wish);
        _response_array[_response_counter++] = static_cast<uint8_t>(_decoded_request.amount);
        _response_array[_response_counter++] = static_cast<uint8_t>(_decoded_request.block);
        _response_array[_response_counter++] = static_cast<uint8_t>(_decoded_request.section);
        _response_array[_response_counter++] = 0;
        _response_array[_response_counter++] = 0;
        _response_array[_response_counter++] = 0;
        _response_array[_response_counter++] = 0;

        send_response();
    }

    return true;
}

bool SysExConf::check_manufacturer_id()
{
    return ((_response_array[static_cast<uint8_t>(ByteOrder::IdByte1)] ==
             _manufacturer_id.id1) &&
            (_response_array[static_cast<uint8_t>(ByteOrder::IdByte2)] ==
             _manufacturer_id.id2) &&
            (_response_array[static_cast<uint8_t>(ByteOrder::IdByte3)] ==
             _manufacturer_id.id3));
}

bool SysExConf::check_status()
{
    return (static_cast<Status>(_response_array[static_cast<uint8_t>(ByteOrder::StatusByte)]) == Status::Request);
}

bool SysExConf::process_special_request()
{
    switch (_response_array[static_cast<uint8_t>(ByteOrder::WishByte)])
    {
    case static_cast<uint8_t>(SpecialRequest::ConnClose):
    {
        if (!_configuration_enabled)
        {
            // The connection cannot be closed if it is not open.
            set_status(Status::ErrorConnection);
            return true;
        }

        // Close the SysEx connection.
        _configuration_enabled = false;
        set_status(Status::Ack);

        return true;
    }
    break;

    case static_cast<uint8_t>(SpecialRequest::ConnOpen):
    {
        // This is necessary to allow the configuration.
        _configuration_enabled = true;
        set_status(Status::Ack);

        return true;
    }
    break;

    case static_cast<uint8_t>(SpecialRequest::BytesPerValue):
    {
        if (_configuration_enabled)
        {
            set_status(Status::Ack);

            _response_array[_response_counter++] = 0;
            _response_array[_response_counter++] = BYTES_PER_VALUE;
        }
        else
        {
            set_status(Status::ErrorConnection);
        }
        return true;
    }
    break;

    case static_cast<uint8_t>(SpecialRequest::ParamsPerMessage):
    {
        if (_configuration_enabled)
        {
            set_status(Status::Ack);

            _response_array[_response_counter++] = 0;
            _response_array[_response_counter++] = PARAMS_PER_MESSAGE;
        }
        else
        {
            set_status(Status::ErrorConnection);
        }

        return true;
    }
    break;

    default:
    {
        // Check for a custom value.
        for (std::size_t i = 0; i < _custom_requests.size(); i++)
        {
            // Check only the current wish/request.
            if (_custom_requests[i].request_id != _response_array[static_cast<uint8_t>(ByteOrder::WishByte)])
            {
                continue;
            }

            if (_configuration_enabled || !_custom_requests[i].conn_open_check)
            {
                set_status(Status::Ack);

                DataHandler::CustomResponse custom_response(std::span<uint8_t>(_response_array, MAX_MESSAGE_SIZE), _response_counter);
                Status                      result = _data_handler.custom_request(_custom_requests[i].request_id, custom_response);

                if (custom_response._overflowed)
                {
                    set_status(Status::ErrorRead);
                    return false;
                }

                switch (result)
                {
                case Status::Ack:
                    break;

                case Status::Request:
                {
                    set_status(Status::ErrorStatus);
                }
                break;

                default:
                {
                    set_status(result);
                    return false;
                }
                break;
                }
            }
            else
            {
                set_status(Status::ErrorConnection);
            }

            return true;
        }

        // Custom request ID not found.
        set_status(Status::ErrorWish);
        return true;
    }
    break;
    }
}

uint16_t SysExConf::generate_message_length()
{
    uint16_t size = 0;

    switch (_decoded_request.amount)
    {
    case Amount::Single:
        return STD_REQ_MIN_MSG_SIZE;

    default:
    {
        // Amount::All path.
        switch (_decoded_request.wish)
        {
        case Wish::Get:
        case Wish::Backup:
            return STD_REQ_MIN_MSG_SIZE;

        default:
        {
            // Wish::Set path.
            size = _layout[_decoded_request.block]
                       ._sections[_decoded_request.section]
                       .number_of_parameters();

            if (size > PARAMS_PER_MESSAGE)
            {
                if ((_decoded_request.part + 1) == _layout[_decoded_request.block]
                                                       ._sections[_decoded_request.section]
                                                       .parts())
                {
                    size = size - ((_layout[_decoded_request.block]
                                        ._sections[_decoded_request.section]
                                        .parts() -
                                    1) *
                                   PARAMS_PER_MESSAGE);
                }
                else
                {
                    size = PARAMS_PER_MESSAGE;
                }
            }

            size *= BYTES_PER_VALUE;
            size += static_cast<uint8_t>(ByteOrder::IndexByte) + 1;

            return size;
        }
        break;
        }
    }
    break;
    }

    return size;
}
bool SysExConf::check_wish()
{
    return (_decoded_request.wish <= Wish::Backup);
}

bool SysExConf::check_amount()
{
    return (_decoded_request.amount <= Amount::All);
}

bool SysExConf::check_block()
{
    return _decoded_request.block < _layout.size();
}

bool SysExConf::check_section()
{
    return (_decoded_request.section < _layout[_decoded_request.block]._sections.size());
}

bool SysExConf::check_part()
{
    if ((_decoded_request.part == PART_ALL_MESSAGES) || (_decoded_request.part == PART_ALL_MESSAGES_WITH_ACK))
    {
        if ((_decoded_request.wish == Wish::Get) ||
            (_decoded_request.wish == Wish::Backup))
        {
            return true;
        }

        return false;
    }

    if (_decoded_request.amount == Amount::All)
    {
        if (_decoded_request.part >= _layout[_decoded_request.block]
                                         ._sections[_decoded_request.section]
                                         .parts())
        {
            return false;
        }

        return true;
    }

    // Do not allow any part other than `0` in single mode.
    if (_decoded_request.part)
    {
        return false;
    }

    return true;
}

bool SysExConf::check_parameter_index()
{
    // The block and section passed validation, so check the parameter index.
    return (_decoded_request.index < _layout[_decoded_request.block]
                                         ._sections[_decoded_request.section]
                                         .number_of_parameters());
}

bool SysExConf::check_new_value()
{
    uint16_t min_value = _layout[_decoded_request.block]
                             ._sections[_decoded_request.section]
                             .new_value_min();
    uint16_t max_value = _layout[_decoded_request.block]
                             ._sections[_decoded_request.section]
                             .new_value_max();

    if (min_value != max_value)
    {
        return ((_decoded_request.new_value >= min_value) &&
                (_decoded_request.new_value <= max_value));
    }

    return true;    // Do not check the new value if min and max are the same.
}

void SysExConf::send_custom_message(std::span<const uint16_t> values, bool ack)
{
    _response_counter = 0;

    _response_array[_response_counter++] = SYSEX_START_BYTE;
    _response_array[_response_counter++] = _manufacturer_id.id1;
    _response_array[_response_counter++] = _manufacturer_id.id2;
    _response_array[_response_counter++] = _manufacturer_id.id3;

    if (ack)
    {
        _response_array[_response_counter++] = static_cast<uint8_t>(Status::Ack);
    }
    else
    {
        _response_array[_response_counter++] = static_cast<uint8_t>(Status::Request);
    }

    _response_array[_response_counter++] = 0;    // Message part.

    for (const auto value : values)
    {
        _response_array[_response_counter++] = value;
    }

    send_response();
}

void SysExConf::send_response()
{
    _response_array[_response_counter++] = SYSEX_END_BYTE;

    const std::span<const uint8_t> response(_response_array, _response_counter);

    if ((response.size() < 2) ||
        (response.front() != SYSEX_START_BYTE) ||
        (response.back() != SYSEX_END_BYTE))
    {
        return;
    }

    /*
     * The assembled response still includes the MIDI 1 SysEx start/end bytes.
     * SysEx7/Data64 UMP packets carry only the payload between those markers.
     */
    const std::span<const uint8_t> payload(response.subspan(1, response.size() - 2));

    (void)midi::write_sysex7_payload_as_ump_packets(0, payload, [&](const midi_ump& packet)
                                                    {
                                                        return _data_handler.send_response(packet);
                                                    });
}

bool SysExConf::add_to_response(uint16_t value)
{
    auto split = Split14Bit(value);

    if (_response_counter >= (MAX_MESSAGE_SIZE - 1))
    {
        return false;
    }

    _response_array[_response_counter++] = split.high();
    _response_array[_response_counter++] = split.low();

    return true;
}
