/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/motor_control/motor_control.h"
#include "zlibs/utils/misc/numeric.h"
#include "zlibs/common/threads.h"

#include <zephyr/logging/log.h>

#include <cmath>

using namespace zlibs::utils::motor_control;

namespace
{
    LOG_MODULE_REGISTER(zlibs_utils_motor, CONFIG_ZLIBS_LOG_LEVEL);
}    // namespace

Motor::Motor(Hwa& hwa, Config config)
    : _hwa(hwa)
    , _config(config)
    , _kwork_reset_movement([this]()
                            {
                                StopCb stop_cb = nullptr;

                                {
                                    const zlibs::utils::misc::LockGuard movement_lock(_movement_lock);

                                    // Use kwork to avoid calling functions outside this module from an interrupt.
                                    if (_run_state.load() != RunState::Stopped)
                                    {
                                        return;
                                    }

                                    if (_config.use_step_toggle)
                                    {
                                        // The state is unknown, so make sure it is off.
                                        _hwa.write_step(false);
                                    }

                                    _hwa.disable();

                                    const zlibs::utils::misc::LockGuard callback_lock(_callback_lock);
                                    stop_cb = _stop_cb;
                                }

                                if (stop_cb != nullptr)
                                {
                                    stop_cb();
                                }
                            },
                            []()
                            {
                                return zlibs::common::internal::Workqueue::handle();
                            })
{}

bool Motor::init()
{
    // Create the shared workqueue here so it is not first created from ISR context.
    [[maybe_unused]] auto handle = zlibs::common::internal::Workqueue::handle();

    if (!_hwa.init())
    {
        LOG_ERR("IO initialization failed");
        return false;
    }

    _run_state                  = RunState::Stopped;
    _timer_interval_us          = 0;
    _acceleration_counter       = 0;
    _movement_pulses_sent       = 0;
    _movement                   = {};
    _stop_requested             = false;
    _pulses_before_deceleration = 0;
    _timer_counter              = 0;
    _run_start_time             = 0;

    {
        const zlibs::utils::misc::LockGuard callback_lock(_callback_lock);
        _init_cb = nullptr;
        _stop_cb = nullptr;
    }

    return true;
}

void Motor::register_init_callback(const InitCb& init_cb)
{
    const zlibs::utils::misc::LockGuard callback_lock(_callback_lock);
    _init_cb = init_cb;
}

bool Motor::register_stop_callback(const StopCb& stop_cb)
{
    const zlibs::utils::misc::LockGuard movement_lock(_movement_lock);

    if (movement_state() != State::Stopped)
    {
        LOG_ERR("Cannot register stop callback while the motor is moving");
        return false;
    }

    const zlibs::utils::misc::LockGuard callback_lock(_callback_lock);
    _stop_cb = stop_cb;

    return true;
}

State Motor::movement_state()
{
    if (_stop_requested.load())
    {
        return State::Stopping;
    }

    return _run_state.load() != RunState::Stopped ? State::Moving : State::Stopped;
}

bool Motor::setup_movement(Movement& info)
{
    const zlibs::utils::misc::LockGuard movement_lock(_movement_lock);

    if (movement_state() != State::Stopped)
    {
        LOG_ERR("Movement setup failed - motor movement already active");
        return false;
    }

    InitCb init_cb = nullptr;
    {
        const zlibs::utils::misc::LockGuard callback_lock(_callback_lock);
        init_cb = _init_cb;
    }

    if (_config.start_interval_us == _config.end_interval_us)
    {
        LOG_ERR("Invalid speed configuration");
        return false;
    }

    if (init_cb != nullptr)
    {
        if (!init_cb(info))
        {
            LOG_ERR("Movement setup failed - initialization check callback returned false");
            return false;
        }
    }

    const auto lower = std::min(_config.start_interval_us, _config.end_interval_us);
    const auto upper = std::max(_config.start_interval_us, _config.end_interval_us);
    const auto speed = static_cast<float>(info.speed);

    if ((speed < lower) || (speed > upper))
    {
        LOG_ERR("Invalid speed provided: %d", static_cast<int>(info.speed));
        return false;
    }

    LOG_DBG("Printing accel curve");

    for (int i = 0; i < static_cast<int>(_accel_lookup_table.size()); i++)
    {
        const auto  midpoint = static_cast<float>(_accel_lookup_table.size()) / 2.0F;
        const auto  position = static_cast<float>(i) - midpoint;
        const float fraction = 1.0F / (1.0F + std::exp(-_config.k_curve * position));
        const auto  interval = static_cast<uint32_t>(_config.start_interval_us - (_config.start_interval_us - speed) * fraction);

        _accel_lookup_table.at(i) = misc::round_to_nearest(interval, TIMER_INTERVAL_US);
        LOG_DBG("[%d]: %d", i, static_cast<int>(_accel_lookup_table.at(i)));
    }

    if (info.pulses)
    {
        _pulses_before_deceleration = info.pulses - (info.pulses / 2);
    }
    else
    {
        _pulses_before_deceleration = 0;
    }

    if (!_hwa.enable())
    {
        LOG_ERR("Enabling motor failed");
        return false;
    }

    if (!write_direction(info.direction))
    {
        if (!_hwa.disable())
        {
            LOG_ERR("Disabling motor after direction failure failed");
        }

        return false;
    }

    _kwork_reset_movement.cancel();

    _movement             = info;
    _timer_interval_us    = _accel_lookup_table.at(0);
    _acceleration_counter = 0;
    _movement_pulses_sent = 0;
    _timer_counter        = 0;
    _run_start_time       = k_uptime_get();
    _stop_requested       = false;
    _run_state            = RunState::Accelerating;

    return true;
}

void Motor::calculate_and_send_next_pulse()
{
    _timer_counter += TIMER_INTERVAL_US;

    if (_timer_counter >= _timer_interval_us.load())
    {
        _timer_counter = 0;

        if (!calculate_next_pulse())
        {
            return;
        }

        if (!send_pulse())
        {
            LOG_ERR("Sending pulse failed");
            reset_movement();
            return;
        }

        _movement_pulses_sent++;
    }
}

bool Motor::calculate_next_pulse()
{
    int32_t new_timer_interval = _timer_interval_us;

    switch (_run_state.load())
    {
    case RunState::Accelerating:
    {
        _acceleration_counter++;

        if (static_cast<uint32_t>(_acceleration_counter) >= _accel_lookup_table.size())
        {
            _acceleration_counter = static_cast<int32_t>(_accel_lookup_table.size() - 1);
            _run_state            = RunState::Running;
        }

        new_timer_interval = _accel_lookup_table.at(_acceleration_counter);
    }
    break;

    case RunState::Running:
    {
        if (_movement.pulses)
        {
            if (_movement_pulses_sent.load() >= _pulses_before_deceleration.load())
            {
                // Start decelerating.
                _run_state = RunState::Decelerating;
            }
        }
    }
    break;

    case RunState::Decelerating:
    {
        _acceleration_counter--;

        if (_acceleration_counter > 0)
        {
            new_timer_interval = _accel_lookup_table.at(_acceleration_counter);
        }
        else
        {
            _run_state = RunState::Stopped;
        }
    }
    break;

    default:
        break;
    }

    _timer_interval_us = new_timer_interval;

    if (_movement.max_run_time)
    {
        if (run_time() >= _movement.max_run_time)
        {
            _run_state = RunState::Stopped;
        }
    }

    if (_movement.pulses)
    {
        if (_movement_pulses_sent.load() >= _movement.pulses)
        {
            _run_state = RunState::Stopped;
        }
    }

    if (_run_state.load() == RunState::Stopped)
    {
        reset_movement();
        return false;
    }

    return true;
}

bool Motor::send_pulse()
{
    if (_config.use_step_toggle)
    {
        return _hwa.toggle_step();
    }

    if (!_hwa.write_step(true))
    {
        return false;
    }

    return _hwa.write_step(false);
}

bool Motor::write_direction(Direction direction)
{
    if (!_hwa.set_direction(direction))
    {
        LOG_ERR("Setting direction failed");
        return false;
    }

    return true;
}

bool Motor::stop_movement(bool force)
{
    if ((movement_state() == State::Stopped) || (movement_state() == State::Stopping))
    {
        return false;
    }

    if (force)
    {
        reset_movement();
    }
    else
    {
        if (!_stop_requested.load())
        {
            _stop_requested = true;

            switch (_run_state.load())
            {
            case RunState::Accelerating:
            case RunState::Running:
            {
                _run_state = RunState::Decelerating;
            }
            break;

            default:
                break;
            }
        }
        else
        {
            // Stopping has already been requested.
            return false;
        }
    }

    return true;
}

void Motor::reset_movement()
{
    LOG_INF("Resetting movement");

    _run_state                  = RunState::Stopped;
    _timer_interval_us          = 0;
    _movement_pulses_sent       = 0;
    _acceleration_counter       = 0;
    _movement                   = {};
    _stop_requested             = false;
    _pulses_before_deceleration = 0;
    _timer_counter              = 0;
    _run_start_time             = 0;

    _kwork_reset_movement.reschedule(0);
}

Config Motor::config() const
{
    return _config;
}

uint32_t Motor::run_time() const
{
    if (!_run_start_time.load())
    {
        // The motor is not running.
        return 0;
    }

    return k_uptime_get() - _run_start_time.load();
}
