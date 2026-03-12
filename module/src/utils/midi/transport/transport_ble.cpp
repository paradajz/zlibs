/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/midi/transport/ble/transport_ble.h"

#include <zephyr/kernel.h>

using namespace zlibs::utils::midi::ble;

namespace
{
    /** @brief Mask for the 13-bit BLE-MIDI timestamp field. */
    constexpr uint16_t BLE_TIMESTAMP_MASK = 0x1FFF;

    /** @brief Bit shift for BLE-MIDI high timestamp bits. */
    constexpr uint8_t BLE_TIMESTAMP_HIGH_SHIFT = 7;

    /** @brief Mask for BLE-MIDI 7-bit timestamp/data payload. */
    constexpr uint8_t BLE_DATA_MASK = 0x7F;
}    // namespace

bool Ble::Transport::init()
{
    _parser.reset();
    _rx_packet_active    = false;
    _rx_packet_index     = 0;
    _rx_search_timestamp = true;
    _rx_sysex_active     = false;

    return _ble._hwa.init();
}

bool Ble::Transport::deinit()
{
    return _ble._hwa.deinit();
}

void Ble::Transport::start_packet()
{
    const auto TIME = k_uptime_get() & BLE_TIMESTAMP_MASK;

    _tx_buffer.size                       = 0;
    _tx_buffer.data.at(_tx_buffer.size++) = (TIME >> BLE_TIMESTAMP_HIGH_SHIFT) | MIDI_STATUS_MIN;
    _tx_buffer.data.at(_tx_buffer.size++) = (TIME & BLE_DATA_MASK) | MIDI_STATUS_MIN;
}

bool Ble::Transport::write_midi_byte(uint8_t data)
{
    if (_tx_buffer.size >= _tx_buffer.data.size())
    {
        if (!flush_packet())
        {
            return false;
        }

        start_packet();
    }

    _tx_buffer.data.at(_tx_buffer.size++) = data;

    return true;
}

bool Ble::Transport::flush_packet()
{
    if (_tx_buffer.size <= 2)
    {
        return true;
    }

    return _ble._hwa.write(_tx_buffer);
}

bool Ble::Transport::write(const midi_ump& packet)
{
    start_packet();

    if (!midi::write_ump_as_midi1_bytes(packet,
                                        UMP_GROUP(packet),
                                        [this](uint8_t data)
                                        {
                                            return write_midi_byte(data);
                                        }))
    {
        return false;
    }

    return flush_packet();
}

std::optional<midi_ump> Ble::Transport::read()
{
    return midi::read_midi1_bytes_as_ump(_parser,
                                         DEFAULT_RX_GROUP,
                                         [this]()
                                         {
                                             return read_midi_byte();
                                         });
}

std::optional<uint8_t> Ble::Transport::read_midi_byte()
{
    while (true)
    {
        if (!_rx_packet_active)
        {
            auto packet = _ble._hwa.read();

            if (!packet.has_value())
            {
                return {};
            }

            _rx_packet           = packet.value();
            _rx_packet_index     = 1;
            _rx_packet_active    = true;
            _rx_search_timestamp = true;
        }

        if (_rx_packet.size > _rx_packet.data.size())
        {
            _rx_packet_active = false;
            continue;
        }

        while (_rx_packet_index < _rx_packet.size)
        {
            const size_t  INDEX = _rx_packet_index++;
            const uint8_t DATA  = _rx_packet.data.at(INDEX);

            if (_rx_search_timestamp)
            {
                if (!(DATA & MIDI_STATUS_MIN))
                {
                    if (INDEX == 1)
                    {
                        _rx_search_timestamp = false;
                        return DATA;
                    }

                    break;
                }

                _rx_search_timestamp = false;
                continue;
            }

            if (DATA == SYS_EX_START)
            {
                _rx_sysex_active = true;
            }
            else if (DATA == SYS_EX_END)
            {
                _rx_sysex_active = false;
            }

            if (!_rx_sysex_active && (INDEX < (_rx_packet.size - 1)))
            {
                if (_rx_packet.data.at(INDEX + 1) & MIDI_STATUS_MIN)
                {
                    _rx_search_timestamp = true;
                }
            }

            return DATA;
        }

        _rx_packet_active = false;
    }
}
