/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/misc/kwork_delayable.h"

#include <zephyr/sys/__assert.h>

#include <array>
#include <concepts>
#include <cstdint>
#include <cmath>
#include <limits>
#include <optional>

namespace zlibs::utils::filters
{
    /**
     * @brief Exponential moving average filter.
     *
     * @tparam T Numeric sample type.
     * @tparam Percentage Smoothing percentage in range `[0, 100]`.
     */
    template<std::unsigned_integral T, uint32_t Percentage>
    class EmaFilter
    {
        public:
        static constexpr uint32_t PERCENTAGE_SCALE = 100;
        static_assert(Percentage <= PERCENTAGE_SCALE, "EMA percentage must be in range [0, 100].");

        EmaFilter() = default;

        /**
         * @brief Feeds one sample and returns filtered value.
         *
         * @param raw Raw sample.
         *
         * @return Filtered output value.
         */
        T value(T raw)
        {
            _current_value = (PERCENTAGE * static_cast<uint32_t>(raw) + (PERCENTAGE_SCALE - PERCENTAGE) * static_cast<uint32_t>(_current_value)) / PERCENTAGE_SCALE;
            return _current_value;
        }

        /**
         * @brief Resets filter internal state.
         */
        void reset()
        {
            _current_value = 0;
        }

        private:
        static constexpr uint32_t PERCENTAGE     = Percentage;
        T                         _current_value = 0;
    };

    /**
     * @brief Median filter returning one stable value every `size` samples.
     *
     * @tparam T Numeric sample type.
     * @tparam Size Sample window size (must be odd).
     */
    template<typename T, size_t Size>
    class MedianFilter
    {
        static_assert(Size % 2 != 0, "Median filter size must be odd.");

        public:
        MedianFilter() = default;

        /**
         * @brief Adds one sample and computes median when window is full.
         *
         * @param raw Raw sample.
         *
         * @return Median value when available; otherwise `std::nullopt`.
         */
        std::optional<T> filter_value(T raw)
        {
            auto compare = [](const void* a, const void* b)
            {
                if (*(T*)a < *(T*)b)
                {
                    return -1;
                }

                if (*(T*)a > *(T*)b)
                {
                    return 1;
                }

                return 0;
            };

            _analog_sample[_median_sample_counter++] = raw;

            // Take the median value to avoid using outliers.
            if (_median_sample_counter == MEDIAN_SAMPLE_COUNT)
            {
                qsort(_analog_sample, MEDIAN_SAMPLE_COUNT, sizeof(T), compare);
                _median_sample_counter = 0;
                _last_stable_value     = _analog_sample[MEDIAN_MIDDLE_VALUE];
                return _last_stable_value;
            }

            return {};
        }

        /**
         * @brief Returns last computed median value.
         *
         * @return Last stable median output.
         */
        T value() const
        {
            return _last_stable_value;
        }

        /**
         * @brief Clears collected samples.
         */
        void reset()
        {
            _median_sample_counter = 0;
            _last_stable_value     = 0;
        }

        private:
        static constexpr size_t MEDIAN_SAMPLE_COUNT = Size;
        static constexpr size_t MEDIAN_MIDDLE_VALUE = ((Size + 1) / 2) - 1;

        T      _analog_sample[MEDIAN_SAMPLE_COUNT] = {};
        T      _last_stable_value                  = 0;
        size_t _median_sample_counter              = 0;
    };

    /**
     * @brief Constraint describing the static configuration required by `AnalogFilter`.
     *
     * @tparam T Configuration type.
     */
    template<typename T>
    concept FilterConfig = requires {
        // Minimum required change in signal value between two reads once the slow filter is active
        { T::MIN_RAW_READING_DIFF_SLOW } -> std::convertible_to<uint16_t>;

        // Minimum required change in signal value between two reads once the fast filter is active
        { T::K_MIN_RAW_READING_DIFF_FAST } -> std::convertible_to<uint16_t>;

        // Time in milliseconds after which slow filter is active on inactivity
        { T::SLOW_FILTER_ENABLE_AFTER_INACTIVE_MS } -> std::convertible_to<uint32_t>;

        // Minimum required number of readings for median filter
        { T::MEDIAN_FILTER_NUMBER_OF_READINGS } -> std::convertible_to<uint16_t>;

        // Percentage applied to the exponential moving average filter
        { T::EMA_FILTER_SMOOTHING_PERCENTAGE } -> std::convertible_to<uint16_t>;

        // When true, analog filtering is disabled and raw value is returned immediately
        { T::ENABLED } -> std::convertible_to<bool>;
    };

    /**
     * @brief Multi-channel analog filter combining threshold, median and EMA stages.
     *
     * @tparam Config Static configuration type satisfying `FilterConfig`.
     * @tparam Size Number of independent channels.
     */
    template<FilterConfig Config, size_t Size>
    class AnalogFilter
    {
        public:
        /**
         * @brief Creates a filter bank and resets all channels to initial state.
         */
        AnalogFilter()
        {
            for (size_t i = 0; i < Size; i++)
            {
                reset(i);
            }
        }

        /**
         * @brief Filters one raw sample for a specific channel.
         *
         * @param index Channel index.
         * @param raw_value Raw ADC/sample value.
         *
         * @return New filtered output when changed; otherwise `std::nullopt`.
         */
        std::optional<uint16_t> filter_value(size_t index, uint16_t raw_value)
        {
            __ASSERT(index < Size, "Index out of range");

            if constexpr (!Config::ENABLED)
            {
                return raw_value;
            }

            const bool     direction        = raw_value >= _last_stable_raw_value.at(index);
            const bool     slow_filter      = ((k_uptime_get() - _last_stable_movement_time.at(index)) > Config::SLOW_FILTER_ENABLE_AFTER_INACTIVE_MS) || (direction != _last_stable_direction.at(index));
            const uint16_t min_reading_diff = slow_filter ? Config::MIN_RAW_READING_DIFF_SLOW : Config::K_MIN_RAW_READING_DIFF_FAST;
            const auto     diff             = abs(raw_value - _last_stable_raw_value.at(index));

            if (diff < min_reading_diff)
            {
                return {};
            }

            auto median = _median_filter.at(index).filter_value(raw_value);

            if (!median.has_value())
            {
                // Not enough samples yet.
                return {};
            }

            auto ema = _ema_filter.at(index).value(median.value());

            if (_last_stable_filtered_value.at(index) != ema)
            {
                _last_stable_raw_value.at(index)      = raw_value;
                _last_stable_filtered_value.at(index) = ema;
                _last_stable_movement_time.at(index)  = k_uptime_get();
                _last_stable_direction.at(index)      = direction;
                return ema;
            }

            return {};
        }

        /**
         * @brief Returns the last stable filtered value for a channel.
         *
         * @param index Channel index.
         *
         * @return Last filtered value.
         */
        uint16_t value(size_t index)
        {
            __ASSERT(index < Size, "Index out of range");

            return _last_stable_filtered_value.at(index);
        }

        /**
         * @brief Resets one channel state.
         *
         * @param index Channel index.
         */
        void reset(size_t index)
        {
            __ASSERT(index < Size, "Index out of range");

            _ema_filter.at(index).reset();
            _median_filter.at(index).reset();
            _last_stable_raw_value.at(index)      = 0;
            _last_stable_filtered_value.at(index) = 0;
            _last_stable_movement_time.at(index)  = 0;
            _last_stable_direction.at(index)      = false;
        }

        private:
        /** @brief EMA filter type instantiated from the provided static config. */
        using EmaFilterType = EmaFilter<uint16_t, Config::EMA_FILTER_SMOOTHING_PERCENTAGE>;

        /** @brief Median filter type instantiated from the provided static config. */
        using MedianFilterType = MedianFilter<uint16_t, Config::MEDIAN_FILTER_NUMBER_OF_READINGS>;

        std::array<EmaFilterType, Size>    _ema_filter                 = {};
        std::array<MedianFilterType, Size> _median_filter              = {};
        std::array<uint16_t, Size>         _last_stable_raw_value      = {};
        std::array<uint16_t, Size>         _last_stable_filtered_value = {};
        std::array<uint32_t, Size>         _last_stable_movement_time  = {};
        std::array<bool, Size>             _last_stable_direction      = {};
    };

    /**
     * @brief Debouncer for boolean input signals.
     */
    class Debounce
    {
        public:
        /**
         * @brief Creates debouncer.
         *
         * @param debounce_time_ms Debounce interval in milliseconds.
         */
        explicit Debounce(uint32_t debounce_time_ms);

        /**
         * @brief Updates debouncer with a new raw state.
         *
         * @param state Current raw signal state.
         */
        void update(bool state)
        {
            if (state != _new_state)
            {
                _new_state = state;

                // If debouncing is disabled, use the new value as the stable one.
                if (_debounce_time_ms)
                {
                    _kwork_update.reschedule(_debounce_time_ms);
                }
                else
                {
                    _stable_state = state;
                }
            }
        }

        /**
         * @brief Returns current debounced state.
         *
         * @return Stable state.
         */
        bool state()
        {
            return _stable_state;
        }

        /**
         * @brief Returns configured debounce interval.
         *
         * @return Debounce interval in milliseconds.
         */
        uint32_t debounce_time_ms() const
        {
            return _debounce_time_ms;
        }

        /**
         * @brief Sets debounce interval.
         *
         * @param debounce_time_ms New debounce interval in milliseconds.
         */
        void set_debounce_time_ms(uint32_t debounce_time_ms)
        {
            _debounce_time_ms = debounce_time_ms;
        }

        private:
        static constexpr uint32_t          DEFAULT_DEBOUNCE_TIME_MS = 10;
        uint32_t                           _debounce_time_ms        = DEFAULT_DEBOUNCE_TIME_MS;
        bool                               _stable_state            = false;
        bool                               _new_state               = false;
        zlibs::utils::misc::KworkDelayable _kwork_update;
    };
}    // namespace zlibs::utils::filters
