/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "deps.h"
#include "zlibs/utils/misc/kwork_delayable.h"
#include "zlibs/utils/misc/mutex.h"

#include <atomic>

namespace zlibs::utils::motor_control
{
    class MotorControl;

    /**
     * @brief Single-motor runtime controller used by `MotorControl`.
     */
    class Motor : public zlibs::utils::misc::NonCopyableOrMovable
    {
        public:
        /**
         * @brief Constructs a motor controller instance.
         *
         * @param hwa Hardware abstraction implementation.
         * @param config Motion profile configuration.
         */
        explicit Motor(Hwa& hwa, Config config);

        private:
        friend class MotorControl;

        /**
         * @brief Internal state machine used while generating pulses.
         */
        enum class RunState : uint8_t
        {
            Stopped,
            Accelerating,
            Running,
            Decelerating,
        };

        static constexpr size_t MAX_ACC_STEPS = 128;

        Hwa&                                _hwa;
        Config                              _config;
        std::atomic<RunState>               _run_state                  = RunState::Stopped;
        std::atomic<int32_t>                _timer_interval_us          = 0;
        std::atomic<int32_t>                _acceleration_counter       = 0;
        std::atomic<uint32_t>               _movement_pulses_sent       = 0;
        Movement                            _movement                   = {};
        InitCb                              _init_cb                    = nullptr;
        StopCb                              _stop_cb                    = nullptr;
        std::atomic<bool>                   _stop_requested             = false;
        std::atomic<uint32_t>               _pulses_before_deceleration = 0;
        std::array<uint16_t, MAX_ACC_STEPS> _accel_lookup_table         = {};
        int32_t                             _timer_counter              = 0;
        std::atomic<uint32_t>               _run_start_time             = 0;
        zlibs::utils::misc::Mutex           _callback_lock;
        zlibs::utils::misc::Mutex           _movement_lock;
        zlibs::utils::misc::KworkDelayable  _kwork_reset_movement;

        /**
         * @brief Initializes motor hardware and internal state.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init();

        /**
         * @brief Registers movement-initialization callback.
         *
         * @param init_cb Callback invoked before movement starts.
         */
        void register_init_callback(const InitCb& init_cb);

        /**
         * @brief Registers movement-stop callback.
         *
         * @param stop_cb Callback invoked after movement stops.
         *
         * @return `true` when the callback is registered, otherwise `false`.
         */
        bool register_stop_callback(const StopCb& stop_cb);

        /**
         * @brief Returns current movement state.
         *
         * @return Current movement state.
         */
        State movement_state();

        /**
         * @brief Configures and starts motor movement.
         *
         * @param info Movement specification.
         *
         * @return `true` when setup succeeds, otherwise `false`.
         */
        bool setup_movement(Movement& info);

        /**
         * @brief Requests stopping of movement.
         *
         * @param force When `true`, performs immediate stop without deceleration.
         *
         * @return `true` if stop request is accepted, otherwise `false`.
         */
        bool stop_movement(bool force = false);

        /**
         * @brief Returns motor configuration.
         *
         * @return Motor configuration copy.
         */
        Config config() const;

        /**
         * @brief Returns elapsed runtime for current movement.
         *
         * @return Runtime in milliseconds.
         */
        uint32_t run_time() const;

        /**
         * @brief Writes motor direction to hardware.
         *
         * @param direction Requested direction.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool write_direction(Direction direction);

        /**
         * @brief Calculates next pulse timing/state.
         *
         * @return `true` while movement should continue, otherwise `false`.
         */
        bool calculate_next_pulse();

        /**
         * @brief Sends one motor step pulse.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_pulse();

        /**
         * @brief Resets movement-related runtime state.
         */
        void reset_movement();

        /**
         * @brief Computes next pulse parameters and emits pulse when due.
         */
        void calculate_and_send_next_pulse();
    };
}    // namespace zlibs::utils::motor_control
