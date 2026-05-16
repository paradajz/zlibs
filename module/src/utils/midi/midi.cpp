/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/midi/midi.h"

using namespace zlibs::utils::midi;

bool Base::enable_thru_route([[maybe_unused]] Thru& destination)
{
    return true;
}

void Base::disable_thru_route([[maybe_unused]] Thru& destination)
{}

bool Base::has_thru_bypass([[maybe_unused]] Thru& destination)
{
    return false;
}

bool Base::init()
{
    if (_initialized)
    {
        return true;
    }

    if ((_transport != nullptr) && _transport->init())
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

    {
        const zlibs::utils::misc::LockGuard lock(_thru_mutex);

        for (auto& interface : _thru_interfaces)
        {
            if (interface == nullptr)
            {
                continue;
            }

            disable_thru_route(*interface);
            interface = nullptr;
        }
    }

    return (_transport != nullptr) && _transport->deinit();
}

bool Base::initialized()
{
    return _initialized;
}

bool Base::supported()
{
    return (_transport != nullptr) && _transport->supported();
}

bool Base::send(const midi_ump& packet)
{
    const zlibs::utils::misc::LockGuard lock(_tx_mutex);
    return (_transport != nullptr) && _transport->write(packet);
}

std::optional<midi_ump> Base::read()
{
    if (_transport == nullptr)
    {
        return {};
    }

    auto packet = _transport->read();

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
        const zlibs::utils::misc::LockGuard lock(_thru_mutex);
        thru_interfaces = _thru_interfaces;
    }

    for (size_t i = 0; i < thru_interfaces.size(); i++)
    {
        auto interface = thru_interfaces.at(i);

        if (interface == nullptr)
        {
            continue;
        }

        if (has_thru_bypass(*interface))
        {
            continue;
        }

        interface->write(packet);
    }
}

bool Base::register_thru_interface(Thru& interface_ref)
{
    const zlibs::utils::misc::LockGuard lock(_thru_mutex);

    for (const auto* interface : _thru_interfaces)
    {
        if (interface == &interface_ref)
        {
            return true;
        }
    }

    if (!enable_thru_route(interface_ref))
    {
        return false;
    }

    for (size_t i = 0; i < _thru_interfaces.size(); i++)
    {
        if (_thru_interfaces.at(i) == nullptr)
        {
            _thru_interfaces[i] = &interface_ref;
            return true;
        }
    }

    disable_thru_route(interface_ref);
    return false;
}

void Base::unregister_thru_interface(Thru& interface_ref)
{
    const zlibs::utils::misc::LockGuard lock(_thru_mutex);

    disable_thru_route(interface_ref);

    for (size_t i = 0; i < _thru_interfaces.size(); i++)
    {
        if (_thru_interfaces.at(i) == &interface_ref)
        {
            _thru_interfaces[i] = nullptr;
        }
    }
}
