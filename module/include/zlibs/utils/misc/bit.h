/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace zlibs::utils::misc
{
    /** @brief Number of bits in one byte/octet. */
    constexpr inline uint8_t BYTE_BIT_COUNT = 8U;

    /** @brief Number of bits in one nibble. */
    constexpr inline uint8_t NIBBLE_BIT_COUNT = 4U;

    /** @brief Number of nibbles in one byte. */
    constexpr inline uint8_t BYTE_NIBBLE_COUNT = 2U;

    /** @brief Number of bytes in an 8-bit value. */
    constexpr inline uint8_t BYTE_SIZE_IN_BYTES = 1U;

    /** @brief Number of bytes in a 16-bit word. */
    constexpr inline uint8_t WORD_SIZE_IN_BYTES = 2U;

    /** @brief Number of bytes in a 32-bit double word. */
    constexpr inline uint8_t DWORD_SIZE_IN_BYTES = 4U;

    /** @brief Mask for extracting one byte. */
    constexpr inline uint32_t BYTE_MASK = 0xFFU;

    /** @brief Mask for extracting one 16-bit word. */
    constexpr inline uint32_t WORD_MASK = 0xFFFFU;

    /** @brief Mask for extracting the low nibble from a byte. */
    constexpr inline uint8_t LOW_NIBBLE_MASK = 0x0FU;

    /** @brief Mask for extracting the high nibble from a byte. */
    constexpr inline uint8_t HIGH_NIBBLE_MASK = 0xF0U;

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
