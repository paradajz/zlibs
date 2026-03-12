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

    _initialized = false;
    return _transport.deinit();
}

bool Base::initialized()
{
    return _initialized;
}

bool Base::send(const midi_ump& packet)
{
    const zlibs::utils::misc::LockGuard LOCK(_tx_mutex);
    return _transport.write(packet);
}

std::optional<midi_ump> Base::read()
{
    auto packet = _transport.read();

    if (!packet.has_value())
    {
        return {};
    }

    thru(packet.value());

    return packet;
}

void Base::thru(const midi_ump& packet)
{
    std::array<Thru*, CONFIG_ZLIBS_UTILS_MIDI_MAX_THRU_INTERFACES> thru_interfaces = {};

    {
        const zlibs::utils::misc::LockGuard LOCK(_thru_mutex);
        thru_interfaces = _thru_interfaces;
    }

    for (size_t i = 0; i < thru_interfaces.size(); i++)
    {
        auto interface = thru_interfaces.at(i);

        if (interface == nullptr)
        {
            continue;
        }

        interface->write(packet);
    }
}

void Base::register_thru_interface(Thru& interface_ref)
{
    const zlibs::utils::misc::LockGuard LOCK(_thru_mutex);

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
    const zlibs::utils::misc::LockGuard LOCK(_thru_mutex);

    for (size_t i = 0; i < _thru_interfaces.size(); i++)
    {
        if (_thru_interfaces.at(i) == &interface_ref)
        {
            _thru_interfaces[i] = nullptr;
        }
    }
}
