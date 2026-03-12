/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cinttypes>
#include <cstring>
#include <string_view>

namespace zlibs::utils::settings
{
    /**
     * @brief Detects whether a setting descriptor provides a validation function.
     *
     * @tparam T Setting descriptor type.
     */
    template<typename T>
    concept HasValidate =
        requires(const typename T::Value& d) {
            { T::Properties::validate(d) } -> std::same_as<bool>;
        };

    /**
     * @brief Detects whether a setting descriptor exposes a string-view-compatible key.
     *
     * @tparam T Setting descriptor type.
     */
    template<typename T>
    concept HasStringViewKey =
        requires {
            std::string_view{ T::Properties::KEY };
        };

    /**
     * @brief Constraint describing a supported compile-time setting descriptor.
     *
     * @tparam T Setting descriptor type.
     */
    template<typename T>
    concept SettingData =
        requires {
            typename T::Value;
            requires std::is_trivially_constructible_v<typename T::Value>;
            typename T::Properties;
            requires HasStringViewKey<T>;
            requires std::same_as<std::remove_cvref_t<decltype(T::Properties::DEFAULT_VALUE)>, typename T::Value>;
            { T::Properties::KEEP_ON_RESET } -> std::convertible_to<bool>;
        };

    /**
     * @brief Storage wrapper for one concrete setting descriptor.
     *
     * @tparam T Setting descriptor type.
     */
    template<SettingData T>
    class BaseSetting
    {
        public:
        /**
         * @brief Initializes setting data with its declared default value.
         */
        BaseSetting()
            : _value(T::Properties::DEFAULT_VALUE)
        {}

        /**
         * @brief Returns mutable access to the stored setting value.
         *
         * @return Mutable reference to setting value.
         */
        typename T::Value& value()
        {
            return _value;
        }

        /**
         * @brief Returns read-only access to the stored setting value.
         *
         * @return Constant reference to setting value.
         */
        const typename T::Value& value() const
        {
            return _value;
        }

        /**
         * @brief Updates stored setting value.
         *
         * If `T::Properties::validate()` exists, it must accept the new value.
         *
         * @param new_value New setting value.
         *
         * @return `true` if update succeeds, otherwise `false`.
         */
        bool update(const typename T::Value& new_value)
        {
            if constexpr (HasValidate<T>)
            {
                if (!T::Properties::validate(new_value))
                {
                    return false;
                }
            }

            _value = new_value;
            return true;
        }

        private:
        typename T::Value _value;
    };
}    // namespace zlibs::utils::settings
