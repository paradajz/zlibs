/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/sysex_conf/sysex_conf.h"

using namespace zlibs::utils::sysex_conf;

void SysExConf::reset()
{
    _sys_ex_enabled                 = false;
    _user_error_ignore_mode_enabled = false;
    _decoded_message                = {};
    _response_counter               = 0;
    _layout                         = {};
    _sys_ex_custom_request          = {};
}

bool SysExConf::set_layout(std::span<const Block> layout)
{
    _sys_ex_enabled = false;

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
        _sys_ex_custom_request = custom_requests;

        for (std::size_t i = 0; i < _sys_ex_custom_request.size(); i++)
        {
            if (_sys_ex_custom_request[i].request_id < static_cast<uint8_t>(SpecialRequest::Count))
            {
                _sys_ex_custom_request = {};
                return false;    // id already used internally
            }
        }

        return true;
    }

    return false;
}

bool SysExConf::is_configuration_enabled()
{
    return _sys_ex_enabled;
}

void SysExConf::set_user_error_ignore_mode(bool state)
{
    _user_error_ignore_mode_enabled = state;
}

void SysExConf::handle_message(std::span<const uint8_t> message)
{
    if (_layout.empty())
    {
        return;
    }

    if (message.size() < SPECIAL_REQ_MSG_SIZE)
    {
        return;    // ignore small messages
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

    reset_decoded_message();

    // copy entire incoming message to internal buffer
    for (size_t i = 0; i < message.size(); i++)
    {
        _response_array[i] = message[i];
    }

    // for now, set the response counter to last position in request
    _response_counter = message.size() - 1;

    if (!check_manufacturer_id())
    {
        return;    // don't send response to wrong ID
    }

    bool should_send_response = true;

    if (!check_status())
    {
        set_status(Status::ErrorStatus);
    }
    else
    {
        if (decode(message))
        {
            if (message.size() == SPECIAL_REQ_MSG_SIZE)
            {
                process_special_request();
            }
            else
            {
                if (process_standard_request(message.size()))
                {
                    // in this case, process_standard_request will internally call
                    // send_response function, which means it's not necessary to call
                    // it again here
                    should_send_response = false;
                }
                else
                {
                    // function returned error
                    // send response manually and reset decoded message
                    reset_decoded_message();
                }
            }
        }
        else
        {
            reset_decoded_message();
        }
    }

    if (should_send_response)
    {
        send_response(false);
    }
}

void SysExConf::reset_decoded_message()
{
    _decoded_message.status    = Status::Ack;
    _decoded_message.wish      = Wish::Invalid;
    _decoded_message.amount    = Amount::Invalid;
    _decoded_message.block     = 0;
    _decoded_message.section   = 0;
    _decoded_message.part      = 0;
    _decoded_message.index     = 0;
    _decoded_message.new_value = 0;
}

bool SysExConf::decode(std::span<const uint8_t> message)
{
    if (message.size() == SPECIAL_REQ_MSG_SIZE)
    {
        // special request
        return true;    // checked in process_special_request
    }

    if (message.size() < (STD_REQ_MIN_MSG_SIZE - BYTES_PER_VALUE))    // no index here
    {
        set_status(Status::ErrorMessageLength);
        return false;
    }

    if (!_sys_ex_enabled)
    {
        // connection open request hasn't been received
        set_status(Status::ErrorConnection);
        return false;
    }

    // don't try to request these parameters if the size is too small
    _decoded_message.part    = message[static_cast<uint8_t>(ByteOrder::PartByte)];
    _decoded_message.wish    = static_cast<Wish>(message[static_cast<uint8_t>(ByteOrder::WishByte)]);
    _decoded_message.amount  = static_cast<Amount>(message[static_cast<uint8_t>(ByteOrder::AmountByte)]);
    _decoded_message.block   = message[static_cast<uint8_t>(ByteOrder::BlockByte)];
    _decoded_message.section = message[static_cast<uint8_t>(ByteOrder::SectionByte)];

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

    // start building response
    set_status(Status::Ack);

    if (_decoded_message.amount == Amount::Single)
    {
        auto merged_index = Merge14Bit(
            message[static_cast<uint8_t>(ByteOrder::IndexByte)],
            message[static_cast<uint8_t>(ByteOrder::IndexByte) + 1]);
        _decoded_message.index = merged_index();

        if (_decoded_message.wish == Wish::Set)
        {
            auto merged_new_value =
                Merge14Bit(message[static_cast<uint8_t>(ByteOrder::IndexByte) + BYTES_PER_VALUE],
                           message[static_cast<uint8_t>(ByteOrder::IndexByte) + BYTES_PER_VALUE + 1]);
            _decoded_message.new_value = merged_new_value();
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

    if ((_decoded_message.wish == Wish::Backup) || (_decoded_message.wish == Wish::Get))
    {
        if ((_decoded_message.part == PART_ALL_MESSAGES) || (_decoded_message.part == PART_ALL_MESSAGES_WITH_ACK))
        {
            // when these parts are specified, protocol will loop over all
            // message parts and deliver as many messages as there are parts as
            // response
            msg_parts_loop = _layout[_decoded_message.block]
                                 ._sections[_decoded_message.section]
                                 .parts();
            all_parts_loop = true;

            // When part is set to 126 (0x7E), send final ACK once all parts are sent.
            if (_decoded_message.part == PART_ALL_MESSAGES_WITH_ACK)
            {
                all_parts_ack = true;
            }
        }

        if (_decoded_message.wish == Wish::Backup)
        {
            // convert response to request
            _response_array[static_cast<uint8_t>(ByteOrder::StatusByte)] = static_cast<uint8_t>(Status::Request);
            // Convert response wish to SET.
            _response_array[static_cast<uint8_t>(ByteOrder::WishByte)] = (uint8_t)Wish::Set;
            // Decode path needs GET while retrieving values.
            _decoded_message.wish = Wish::Get;
            // when backup is request, erase received index/new value in response
            response_counter_local = received_array_size - 1 - (2 * BYTES_PER_VALUE);
        }
    }

    for (int j = 0; j < msg_parts_loop; j++)
    {
        _response_counter = response_counter_local;

        if (all_parts_loop)
        {
            _decoded_message.part                                      = j;
            _response_array[static_cast<uint8_t>(ByteOrder::PartByte)] = j;
        }

        if (_decoded_message.amount == Amount::All)
        {
            start_index = PARAMS_PER_MESSAGE * _decoded_message.part;
            end_index   = start_index + PARAMS_PER_MESSAGE;

            if (end_index > _layout[_decoded_message.block]
                                ._sections[_decoded_message.section]
                                .number_of_parameters())
            {
                end_index = _layout[_decoded_message.block]
                                ._sections[_decoded_message.section]
                                .number_of_parameters();
            }
        }

        for (uint16_t i = start_index; i < end_index; i++)
        {
            switch (_decoded_message.wish)
            {
            case Wish::Get:
            {
                if (_decoded_message.amount == Amount::Single)
                {
                    if (!check_parameter_index())
                    {
                        set_status(Status::ErrorIndex);
                        return false;
                    }

                    uint16_t value  = 0;
                    Status   result = _data_handler.get(_decoded_message.block,
                                                        _decoded_message.section,
                                                        _decoded_message.index,
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
                    // get all params - no index is specified
                    uint16_t value  = 0;
                    Status   result = _data_handler.get(_decoded_message.block, _decoded_message.section, i, value);

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
                if (_decoded_message.amount == Amount::Single)
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

                    Status result = _data_handler.set(_decoded_message.block, _decoded_message.section, _decoded_message.index, _decoded_message.new_value);

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
                    _decoded_message.new_value = merge();

                    if (!check_new_value())
                    {
                        set_status(Status::ErrorNewValue);
                        return false;
                    }

                    Status result = _data_handler.set(_decoded_message.block,
                                                      _decoded_message.section,
                                                      i,
                                                      _decoded_message.new_value);

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

        send_response(false);
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
        _response_array[_response_counter++] = static_cast<uint8_t>(_decoded_message.wish);
        _response_array[_response_counter++] = static_cast<uint8_t>(_decoded_message.amount);
        _response_array[_response_counter++] = static_cast<uint8_t>(_decoded_message.block);
        _response_array[_response_counter++] = static_cast<uint8_t>(_decoded_message.section);
        _response_array[_response_counter++] = 0;
        _response_array[_response_counter++] = 0;
        _response_array[_response_counter++] = 0;
        _response_array[_response_counter++] = 0;

        send_response(false);
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
        if (!_sys_ex_enabled)
        {
            // connection can't be closed if it isn't opened
            set_status(Status::ErrorConnection);
            return true;
        }

        // close sysex connection
        _sys_ex_enabled = false;
        set_status(Status::Ack);

        return true;
    }
    break;

    case static_cast<uint8_t>(SpecialRequest::ConnOpen):
    {
        // necessary to allow the configuration
        _sys_ex_enabled = true;
        set_status(Status::Ack);

        return true;
    }
    break;

    case static_cast<uint8_t>(SpecialRequest::BytesPerValue):
    {
        if (_sys_ex_enabled)
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
        if (_sys_ex_enabled)
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
        // check for custom value
        for (std::size_t i = 0; i < _sys_ex_custom_request.size(); i++)
        {
            // check only current wish/request
            if (_sys_ex_custom_request[i].request_id != _response_array[static_cast<uint8_t>(ByteOrder::WishByte)])
            {
                continue;
            }

            if (_sys_ex_enabled || !_sys_ex_custom_request[i].conn_open_check)
            {
                set_status(Status::Ack);

                DataHandler::CustomResponse custom_response(std::span<uint8_t>(_response_array, MAX_MESSAGE_SIZE), _response_counter);
                Status                      result = _data_handler.custom_request(_sys_ex_custom_request[i].request_id, custom_response);

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

    switch (_decoded_message.amount)
    {
    case Amount::Single:
        return STD_REQ_MIN_MSG_SIZE;

    default:
    {
        // Amount::All path.
        switch (_decoded_message.wish)
        {
        case Wish::Get:
        case Wish::Backup:
            return STD_REQ_MIN_MSG_SIZE;

        default:
        {
            // Wish::Set path.
            size = _layout[_decoded_message.block]
                       ._sections[_decoded_message.section]
                       .number_of_parameters();

            if (size > PARAMS_PER_MESSAGE)
            {
                if ((_decoded_message.part + 1) == _layout[_decoded_message.block]
                                                       ._sections[_decoded_message.section]
                                                       .parts())
                {
                    size = size - ((_layout[_decoded_message.block]
                                        ._sections[_decoded_message.section]
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
    return (_decoded_message.wish <= Wish::Backup);
}

bool SysExConf::check_amount()
{
    return (_decoded_message.amount <= Amount::All);
}

bool SysExConf::check_block()
{
    return _decoded_message.block < _layout.size();
}

bool SysExConf::check_section()
{
    return (_decoded_message.section < _layout[_decoded_message.block]._sections.size());
}

bool SysExConf::check_part()
{
    if ((_decoded_message.part == PART_ALL_MESSAGES) || (_decoded_message.part == PART_ALL_MESSAGES_WITH_ACK))
    {
        if ((_decoded_message.wish == Wish::Get) ||
            (_decoded_message.wish == Wish::Backup))
        {
            return true;
        }

        return false;
    }

    if (_decoded_message.amount == Amount::All)
    {
        if (_decoded_message.part >= _layout[_decoded_message.block]
                                         ._sections[_decoded_message.section]
                                         .parts())
        {
            return false;
        }

        return true;
    }

    // do not allow part other than 0 in single mode
    if (_decoded_message.part)
    {
        return false;
    }

    return true;
}

bool SysExConf::check_parameter_index()
{
    // block and section passed validation, check parameter index
    return (_decoded_message.index < _layout[_decoded_message.block]
                                         ._sections[_decoded_message.section]
                                         .number_of_parameters());
}

bool SysExConf::check_new_value()
{
    uint16_t min_value = _layout[_decoded_message.block]
                             ._sections[_decoded_message.section]
                             .new_value_min();
    uint16_t max_value = _layout[_decoded_message.block]
                             ._sections[_decoded_message.section]
                             .new_value_max();

    if (min_value != max_value)
    {
        return ((_decoded_message.new_value >= min_value) &&
                (_decoded_message.new_value <= max_value));
    }

    return true;    // don't check new value if min and max are the same
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

    _response_array[_response_counter++] = 0;    // message part

    for (const auto VALUE : values)
    {
        _response_array[_response_counter++] = VALUE;
    }

    send_response(false);
}

void SysExConf::send_response(bool contains_last_byte)
{
    if (!contains_last_byte)
    {
        _response_array[_response_counter++] = SYSEX_END_BYTE;
    }

    _data_handler.send_response(std::span<const uint8_t>(_response_array, _response_counter));
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

uint8_t SysExConf::blocks() const
{
    return _layout.size();
}

uint8_t SysExConf::sections(uint8_t block_index) const
{
    return _layout[block_index]._sections.size();
}
