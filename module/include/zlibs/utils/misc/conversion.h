/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace zlibs::utils::misc
{
    /**
     * @brief Converts Celsius temperature to Kelvin.
     *
     * @param celsius Temperature in Celsius.
     *
     * @return Temperature in Kelvin.
     */
    constexpr inline float celsius_to_kelvin(float celsius)
    {
        return celsius + static_cast<float>(273.15);
    }

    /**
     * @brief Converts Kelvin temperature to Celsius.
     *
     * @param kelvin Temperature in Kelvin.
     *
     * @return Temperature in Celsius.
     */
    constexpr inline float kelvin_to_celsius(float kelvin)
    {
        return kelvin - static_cast<float>(273.15);
    }
}    // namespace zlibs::utils::misc
