/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "noncopyable.h"
#include "datetime.h"

#include <zephyr/kernel.h>

#include <cstdint>
#include <functional>

namespace zlibs::utils::misc
{
    /**
     * @brief Thin wrapper around a Zephyr timer bound to a C++ callback.
     *
     * The callback is executed from the Zephyr timer expiry handler context.
     */
    class Timer : public NonCopyableOrMovable
    {
        public:
        using TimerFunction = std::function<void()>;

        /**
         * @brief Supported timer operating modes.
         */
        enum class Type : uint8_t
        {
            OneShot,
            Repeating
        };

        /**
         * @brief Creates and initializes timer.
         *
         * @param type Timer mode.
         * @param cb Callback executed when timer expires.
         */
        explicit Timer(Type type, TimerFunction&& cb)
            : _type(type)
            , _cb(std::move(cb))
        {
            k_timer_init(&_timer, [](k_timer* timer)
                         {
                             auto self = static_cast<Timer*>(timer->user_data);

                             if (self == nullptr)
                             {
                                 return;
                             }

                             if (self->_cb != nullptr)
                             {
                                 self->_cb();
                             }
                         },
                         nullptr);

            k_timer_user_data_set(&_timer, this);
        }

        ~Timer()
        {
            stop();
        }

        /**
         * @brief Starts or restarts the timer.
         *
         * `Ms` is the default unit.
         *
         * @param duration Expiry duration.
         * @param unit Time unit for `duration`.
         *
         * @return `true` if timer mode is supported, otherwise `false`.
         */
        bool start(uint32_t duration, TimerUnit unit = TimerUnit::Ms)
        {
            switch (unit)
            {
            case TimerUnit::Us:
                return start(K_USEC(duration));

            case TimerUnit::Ms:
                return start(K_MSEC(duration));

            case TimerUnit::Sec:
                return start(K_SECONDS(duration));

            default:
                return false;
            }
        }

        /**
         * @brief Starts or restarts the timer.
         *
         * @param duration Expiry duration as Zephyr timeout.
         *
         * @return `true` if timer mode is supported, otherwise `false`.
         */
        bool start(k_timeout_t duration)
        {
            k_timer_stop(&_timer);

            switch (_type)
            {
            case Type::OneShot:
            {
                k_timer_start(&_timer,
                              duration,
                              K_NO_WAIT);

                return true;
            }
            break;

            case Type::Repeating:
            {
                k_timer_start(&_timer,
                              duration,
                              duration);

                return true;
            }
            break;

            default:
                return false;
            }
        }

        /**
         * @brief Stops the timer if it is running.
         */
        void stop()
        {
            k_timer_stop(&_timer);
        }

        private:
        Type          _type  = Type::OneShot;
        k_timer       _timer = {};
        TimerFunction _cb    = nullptr;
    };
}    // namespace zlibs::utils::misc
