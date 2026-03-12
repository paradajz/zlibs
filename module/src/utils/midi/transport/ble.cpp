/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/midi/transport/ble/transport_ble.h"

using namespace zlibs::utils::midi::ble;

bool Ble::Transport::init()
{
    _ble.use_recursive_parsing(true);
    return _ble._hwa.init();
}

bool Ble::Transport::deinit()
{
    return _ble._hwa.deinit();
}

bool Ble::Transport::begin_transmission([[maybe_unused]] MessageType type)
{
    // timestamp is 13-bit according to midi ble spec
    auto time = _ble._hwa.time() & BLE_TIMESTAMP_MASK;

    _tx_buffer.data[0] = (time >> BLE_TIMESTAMP_HIGH_SHIFT) | MIDI_STATUS_MIN;
    _tx_buffer.data[1] = (time & BLE_DATA_MASK) | MIDI_STATUS_MIN;
    _low_timestamp     = _tx_buffer.data[1];
    _tx_buffer.size    = 2;

    return true;
}

bool Ble::Transport::write(uint8_t data)
{
    _tx_buffer.data[_tx_buffer.size++] = data;

    if (_tx_buffer.size >= _tx_buffer.data.size())
    {
        bool ret_val    = _ble._hwa.write(_tx_buffer);
        _tx_buffer.size = 1;    // keep header
        return ret_val;
    }

    return true;
}

bool Ble::Transport::end_transmission()
{
    return _ble._hwa.write(_tx_buffer);
}

std::optional<uint8_t> Ble::Transport::read()
{
    if (!_rx_index)
    {
        auto packet = _ble._hwa.read();

        if (!packet.has_value())
        {
            return {};
        }

        if (packet.value().size > CONFIG_ZLIBS_UTILS_MIDI_BLE_MAX_PACKET_SIZE)
        {
            return {};
        }

        size_t index            = 0;
        bool   search_timestamp = true;

        // ignore header
        while (++index < packet.value().size)
        {
            if (search_timestamp)
            {
                if (!(packet.value().data[index] & MIDI_STATUS_MIN))
                {
                    if (index == 1)
                    {
                        // sysex continuation, store
                        _rx_buffer[_rx_index++] = packet.value().data[index];
                    }
                    else
                    {
                        // something is wrong
                        break;
                    }
                }

                // start filling buffer from the next byte
                search_timestamp = false;
            }
            else
            {
                _rx_buffer[_rx_index++] = packet.value().data[index];

                if (index < (packet.value().size - 1))
                {
                    if (packet.value().data[index + 1] & MIDI_STATUS_MIN)
                    {
                        // for the next time
                        search_timestamp = true;
                    }
                }
            }
        }
    }

    if (_rx_index)
    {
        const auto DATA = _rx_buffer[_retrieve_index++];

        if (_retrieve_index == _rx_index)
        {
            _retrieve_index = 0;
            _rx_index       = 0;
        }

        return DATA;
    }

    return {};
}
