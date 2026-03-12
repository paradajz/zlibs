/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace zlibs::utils::misc
{
    /**
     * @brief Constrains a value to the closed interval `[low, high]`.
     *
     * @tparam T Comparable type.
     * @param input Input value.
     * @param low Lower bound.
     * @param high Upper bound.
     *
     * @return Clamped value.
     */
    template<typename T>
    constexpr T constrain(T input, T low, T high)
    {
        if (input < low)
        {
            return low;
        }

        if (input > high)
        {
            return high;
        }

        return input;
    }

    /**
     * @brief Maps a value from one range to another.
     *
     * Input is constrained to input bounds first.
     *
     * @tparam T Numeric type.
     * @param x Input value.
     * @param in_min Input range minimum.
     * @param in_max Input range maximum.
     * @param out_min Output range minimum.
     * @param out_max Output range maximum.
     *
     * @return Mapped value in output range.
     */
    template<typename T>
    constexpr T map_range(T x, T in_min, T in_max, T out_min, T out_max)
    {
        // make sure the input is correct
        x = constrain(x, in_min, in_max);

        // don't bother with mapping in this case
        if ((in_min == out_min) && (in_max == out_max))
        {
            return x;
        }

        if (in_min == in_max)
        {
            // nothing to do
            return x;
        }

        // smaller input range to larger output range
        if ((in_max - in_min) > (out_max - out_min))
        {
            return (x - in_min) * (out_max - out_min + 1) / (in_max - in_min + 1) + out_min;
        }

        // larger input range to smaller output range
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    };

    /**
     * @brief Rounds value to nearest multiple.
     *
     * @param value Input value.
     * @param nearest Step size used for rounding.
     *
     * @return Rounded value.
     */
    constexpr uint32_t round_to_nearest(uint32_t value, uint32_t nearest)
    {
        return ((value + nearest / 2) / nearest) * nearest;
    }
}    // namespace zlibs::utils::misc
