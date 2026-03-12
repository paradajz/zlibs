/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

namespace zlibs::utils::misc
{
    /**
     * @brief Reads one bit from an integer value.
     *
     * @tparam T Integral type.
     * @param value Source value.
     * @param bit Bit index.
     *
     * @return `true` when bit is set, otherwise `false`.
     */
    template<typename T>
    constexpr inline bool bit_read(T value, size_t bit)
    {
        return (value >> bit) & static_cast<T>(1);
    }

    /**
     * @brief Sets one bit in an integer value.
     *
     * @tparam T Integral type.
     * @param value Value to modify.
     * @param bit Bit index.
     */
    template<typename T>
    constexpr inline void bit_set(T& value, size_t bit)
    {
        value |= (static_cast<T>(1) << bit);
    }

    /**
     * @brief Clears one bit in an integer value.
     *
     * @tparam T Integral type.
     * @param value Value to modify.
     * @param bit Bit index.
     */
    template<typename T>
    constexpr inline void bit_clear(T& value, size_t bit)
    {
        value &= ~(static_cast<T>(1) << bit);
    }

    /**
     * @brief Writes one bit in an integer value.
     *
     * @tparam T Integral type.
     * @param value Value to modify.
     * @param bit Bit index.
     * @param bit_value Target bit state.
     */
    template<typename T>
    constexpr inline void bit_write(T& value, size_t bit, bool bit_value)
    {
        bit_value ? bit_set(value, bit) : bit_clear(value, bit);
    }
}    // namespace zlibs::utils::misc
