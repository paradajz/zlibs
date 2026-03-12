/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "motor.h"
#include "zlibs/utils/misc/timer.h"

#include <atomic>
#include <functional>
#include <span>

namespace zlibs::utils::motor_control
{
    /**
     * @brief Multi-motor coordinator and scheduling/timer owner.
     */
    class MotorControl : public zlibs::utils::misc::NonCopyableOrMovable
    {
        public:
        /**
         * @brief Creates a motor controller over a fixed borrowed motor span.
         *
         * @param motors Span containing borrowed motor references. The
         *               referenced motors and span storage must outlive
         *               this controller.
         */
        explicit MotorControl(std::span<std::reference_wrapper<Motor>> motors);

        /**
         * @brief Stops internal timer and releases controller resources.
         */
        ~MotorControl();

        /**
         * @brief Initializes all managed motors.
         *
         * @return `true` when all motors initialize successfully, otherwise `false`.
         */
        bool init();

        /**
         * @brief Registers initialization callback for one motor.
         *
         * @param motor_index Zero-based motor index.
         * @param init_cb Callback invoked before movement starts.
         */
        void register_init_callback(size_t motor_index, const InitCb& init_cb);

        /**
         * @brief Registers stop callback for one motor.
         *
         * @param motor_index Zero-based motor index.
         * @param stop_cb Callback invoked when movement stops.
         *
         * @return `true` when the callback is registered, otherwise `false`.
         */
        bool register_stop_callback(size_t motor_index, const StopCb& stop_cb);

        /**
         * @brief Returns current movement state of one motor.
         *
         * @param motor_index Zero-based motor index.
         *
         * @return Current state.
         */
        State movement_state(size_t motor_index);

        /**
         * @brief Configures and starts a movement for one motor.
         *
         * @param motor_index Zero-based motor index.
         * @param info Movement request data.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool setup_movement(size_t motor_index, Movement& info);

        /**
         * @brief Requests stop for one motor.
         *
         * @param motor_index Zero-based motor index.
         * @param force When `true`, stops immediately.
         *
         * @return `true` if stop request is accepted, otherwise `false`.
         */
        bool stop_movement(size_t motor_index, bool force = false);

        /**
         * @brief Returns motion configuration for one motor.
         *
         * @param motor_index Zero-based motor index.
         *
         * @return Motor configuration copy.
         */
        Config config(size_t motor_index) const;

        /**
         * @brief Returns total number of managed motors.
         *
         * @return Number of motors in controller span.
         */
        size_t total_motors() const;

        private:
        std::span<std::reference_wrapper<Motor>> _motors = {};
        zlibs::utils::misc::Timer                _timer;
        std::atomic<bool>                        _timer_started = false;

        /**
         * @brief Periodic handler that advances active motor movements.
         */
        void check_motors();
    };
}    // namespace zlibs::utils::motor_control
