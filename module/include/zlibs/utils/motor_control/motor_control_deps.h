/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "motor_control_common.h"

namespace zlibs::utils::motor_control
{
    /**
     * @brief Hardware abstraction interface for motor drive pins and enable control.
     */
    class Hwa
    {
        public:
        Hwa()          = default;
        virtual ~Hwa() = default;

        /**
         * @brief Initializes hardware resources.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init() = 0;

        /**
         * @brief Enables motor driver output stage.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool enable() = 0;

        /**
         * @brief Disables motor driver output stage.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool disable() = 0;

        /**
         * @brief Writes explicit step pin state.
         *
         * @param state Step pin logical state.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write_step(bool state) = 0;

        /**
         * @brief Toggles step pin state.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool toggle_step() = 0;

        /**
         * @brief Sets motor rotation direction.
         *
         * @param direction Requested direction.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool set_direction(Direction direction) = 0;
    };

    /**
     * @brief Motion-profile configuration constants for one motor.
     */
    struct Config
    {
        static constexpr float DEFAULT_K_CURVE        = 0.1;
        static constexpr float DEFAULT_START_INTERVAL = 1500.0;
        static constexpr float DEFAULT_END_INTERVAL   = 100.0;

        float k_curve           = DEFAULT_K_CURVE;
        float start_interval_us = DEFAULT_START_INTERVAL;
        float end_interval_us   = DEFAULT_END_INTERVAL;
        bool  use_step_toggle   = false;
    };
}    // namespace zlibs::utils::motor_control
