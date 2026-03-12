/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/motor_control/motor_control.h"

#include <zephyr/logging/log.h>

using namespace zlibs::utils::motor_control;

namespace
{
    LOG_MODULE_REGISTER(zlibs_utils_motor_control, CONFIG_ZLIBS_LOG_LEVEL);
}    // namespace

MotorControl::MotorControl(std::span<std::reference_wrapper<Motor>> motors)
    : _motors(motors)
{}

MotorControl::~MotorControl()
{
    stop_timer();
}

bool MotorControl::init()
{
    LOG_INF("Initializing %d motors", static_cast<int>(_motors.size()));

    for (size_t i = 0; i < _motors.size(); i++)
    {
        if (!_motors[i].get().init())
        {
            LOG_ERR("IO initialization failed for motor %d", static_cast<int>(i));
            return false;
        }
    }

    k_timer_init(&_timer, [](k_timer* timer)
                 {
                     auto self = static_cast<MotorControl*>(timer->user_data);
                     self->check_motors();
                 },
                 nullptr);

    k_timer_user_data_set(&_timer, this);

    return true;
}

void MotorControl::register_init_callback(size_t motor_index, const InitCb& init_cb)
{
    __ASSERT(motor_index < _motors.size(), "Motor index out of range");
    _motors[motor_index].get().register_init_callback(init_cb);
}

bool MotorControl::register_stop_callback(size_t motor_index, const StopCb& stop_cb)
{
    __ASSERT(motor_index < _motors.size(), "Motor index out of range");
    return _motors[motor_index].get().register_stop_callback(stop_cb);
}

State MotorControl::movement_state(size_t motor_index)
{
    __ASSERT(motor_index < _motors.size(), "Motor index out of range");
    return _motors[motor_index].get().movement_state();
}

bool MotorControl::setup_movement(size_t motor_index, Movement& info)
{
    __ASSERT(motor_index < _motors.size(), "Motor index out of range");

    if (_motors[motor_index].get().setup_movement(info))
    {
        if (!_timer_started.exchange(true))
        {
            start_timer();
        }

        return true;
    }

    return false;
}

bool MotorControl::stop_movement(size_t motor_index, bool force)
{
    __ASSERT(motor_index < _motors.size(), "Motor index out of range");

    if (_motors[motor_index].get().stop_movement(force))
    {
        bool all_of = true;

        for (size_t i = 0; i < _motors.size(); i++)
        {
            if (_motors[i].get().movement_state() != State::Stopped)
            {
                all_of = false;
                break;
            }
        }

        if (all_of)
        {
            if (_timer_started.exchange(false))
            {
                stop_timer();
            }
        }

        return true;
    }

    return false;
}

Config MotorControl::config(size_t motor_index) const
{
    __ASSERT(motor_index < _motors.size(), "Motor index out of range");

    return _motors[motor_index].get().config();
}

void MotorControl::check_motors()
{
    bool any_active = false;

    for (size_t i = 0; i < _motors.size(); i++)
    {
        if (_motors[i].get().movement_state() != State::Stopped)
        {
            _motors[i].get().calculate_and_send_next_pulse();
        }

        if (_motors[i].get().movement_state() != State::Stopped)
        {
            any_active = true;
        }
    }

    if (!any_active && _timer_started.exchange(false))
    {
        stop_timer();
    }
}

void MotorControl::start_timer()
{
    k_timer_start(&_timer,
                  K_USEC(TIMER_INTERVAL_US),
                  K_USEC(TIMER_INTERVAL_US));
}

void MotorControl::stop_timer()
{
    k_timer_stop(&_timer);
}

size_t MotorControl::total_motors() const
{
    return _motors.size();
}
