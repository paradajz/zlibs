/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <functional>

namespace zlibs::utils::motor_control
{
    /**
     * @brief Supported motor rotation directions.
     */
    enum class Direction : uint8_t
    {
        Cw,
        Ccw
    };

    /**
     * @brief Motor movement state exposed to callers.
     */
    enum class State : uint8_t
    {
        Stopped,
        Moving,
        Stopping
    };

    /**
     * @brief Movement request parameters for one motor operation.
     */
    struct Movement
    {
        Direction direction  = Direction::Cw;
        uint32_t  speed      = 0;
        uint32_t  pulses     = 0;
        uint32_t  maxRunTime = 0;
    };

    constexpr inline uint32_t TIMER_INTERVAL_US = 100;

    /** @brief Callback invoked before movement starts and allowed to validate the request. */
    using InitCb = std::function<bool(const Movement& movement)>;

    /** @brief Callback invoked after a motor movement stops. */
    using StopCb = std::function<void()>;
}    // namespace zlibs::utils::motor_control
