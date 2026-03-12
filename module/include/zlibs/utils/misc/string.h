/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <cstdlib>
#include <string_view>
#include <optional>

namespace zlibs::utils::misc
{
    /**
     * @brief Compile-time string literal wrapper.
     *
     * @tparam N Literal buffer size including null terminator.
     */
    template<size_t N>
    struct StringLiteral
    {
        constexpr StringLiteral() = default;

        /**
         * @brief Constructs wrapper from string literal.
         *
         * @param str Source string literal.
         */
        explicit constexpr StringLiteral(const char (&str)[N])
        {
            std::copy_n(str, N, value.begin());
        }

        /**
         * @brief Converts to string view excluding null terminator.
         *
         * @return String view of wrapped literal.
         */
        explicit constexpr operator std::string_view() const
        {
            return { value.data(), N - 1 };
        }

        /**
         * @brief Returns the wrapped string as a null-terminated C string.
         *
         * @return Pointer to internal null-terminated character buffer.
         */
        constexpr const char* c_str() const
        {
            return value.data();
        }

        std::array<char, N> value = {};
    };

    /**
     * @brief Concatenates compile-time `StringLiteral` values.
     *
     * @tparam Ns Sizes of input string literals including null terminators.
     * @param strings Input string literals.
     *
     * @return Joined `StringLiteral` without duplicate terminators.
     */
    template<size_t... Ns>
    constexpr auto string_join(const StringLiteral<Ns>&... strings)
    {
        constexpr size_t           TOTAL_CHARS = (... + Ns) - sizeof...(Ns) + 1;
        StringLiteral<TOTAL_CHARS> result;
        size_t                     offset = 0;

        ((std::copy_n(strings.value.begin(), Ns - 1, result.value.begin() + offset), offset += Ns - 1), ...);
        result.value[offset] = '\0';

        return result;
    }

    /**
     * @brief Concatenates string literals by converting them to `StringLiteral`.
     *
     * @tparam Args String-literal argument types.
     * @param args Input string literals.
     *
     * @return Joined `StringLiteral`.
     */
    template<typename... Args>
    constexpr auto string_join(const Args&... args)
    {
        return string_join(StringLiteral{ args }...);
    }

    /**
     * @brief Converts an integer to a StringLiteral at compile time.
     *
     * @tparam V The integer value to convert.
     * @return StringLiteral containing the decimal representation, prefixed with '-' if negative.
     */
    template<int V>
    constexpr auto int_to_string()
    {
        constexpr int  DECIMAL_BASE = 10;
        constexpr bool IS_NEGATIVE  = V < 0;
        constexpr int  ABSOLUTE     = IS_NEGATIVE ? -V : V;

        constexpr int DIGITS = []()
        {
            int digit_count = 0;
            int remaining   = ABSOLUTE;

            do
            {
                remaining /= DECIMAL_BASE;
                ++digit_count;
            } while (remaining > 0);

            return digit_count;
        }();

        constexpr int        TOTAL = DIGITS + (IS_NEGATIVE ? 1 : 0) + 1;
        StringLiteral<TOTAL> result;

        int write_offset = TOTAL - 2;
        int remaining    = ABSOLUTE;

        do
        {
            result.value[write_offset--] = '0' + (remaining % DECIMAL_BASE);
            remaining /= DECIMAL_BASE;
        } while (remaining > 0);

        if constexpr (IS_NEGATIVE)
        {
            result.value[0] = '-';
        }

        result.value[TOTAL - 1] = '\0';

        return result;
    }
}    // namespace zlibs::utils::misc
