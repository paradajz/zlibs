/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/sys/__assert.h>

#include <array>
#include <cstdint>

namespace zlibs::utils::misc
{
    constexpr inline int SECONDS_PER_DAY       = 86400;
    constexpr inline int SECONDS_PER_HOUR      = 3600;
    constexpr inline int SECONDS_PER_MINUTE    = 60;
    constexpr inline int MINUTES_PER_HOUR      = 60;
    constexpr inline int HOURS_PER_DAY         = 24;
    constexpr inline int MONTHS_PER_YEAR       = 12;
    constexpr inline int DAYS_PER_YEAR         = 365;
    constexpr inline int DAYS_PER_LEAP_YEAR    = 366;
    constexpr inline int DAYS_PER_WEEK         = 7;
    constexpr inline int DEFAULT_YEAR          = 2025;
    constexpr inline int JANUARY               = 1;
    constexpr inline int FEBRUARY              = 2;
    constexpr inline int DECEMBER              = 12;
    constexpr inline int FIRST_DAY_OF_MONTH    = 1;
    constexpr inline int MONDAY                = 1;
    constexpr inline int TUESDAY               = 2;
    constexpr inline int WEDNESDAY             = 3;
    constexpr inline int THURSDAY              = 4;
    constexpr inline int FRIDAY                = 5;
    constexpr inline int SATURDAY              = 6;
    constexpr inline int SUNDAY                = 7;
    constexpr inline int DAYS_IN_LEAP_FEBRUARY = 29;
    constexpr inline int DEFAULT_WEEKDAY       = 3;
    constexpr inline int LEAP_YEAR_FACTOR      = 4;
    constexpr inline int CENTURY_YEARS         = 100;
    constexpr inline int GREGORIAN_YEARS       = 400;
    constexpr inline int ZELLER_FACTOR         = 13;
    constexpr inline int ZELLER_DIVISOR        = 5;
    constexpr inline int ZELLER_CENTURY_FACTOR = 2;

    enum class TimerUnit : uint8_t
    {
        Us,
        Ms,
        Sec,
    };

    /**
     * @brief Checks if a year is leap according to Gregorian rules.
     *
     * @param year Year number.
     *
     * @return `true` for leap year, otherwise `false`.
     */
    constexpr bool is_leap_year(int year)
    {
        if (year % LEAP_YEAR_FACTOR == 0)
        {
            if (year % CENTURY_YEARS == 0)
            {
                if (year % GREGORIAN_YEARS == 0)
                {
                    return true;
                }

                return false;
            }

            return true;
        }

        return false;
    }

    /**
     * @brief Returns days in a year.
     *
     * @param year Year number.
     *
     * @return `366` for leap years, otherwise `365`.
     */
    constexpr int days_in_year(int year)
    {
        return is_leap_year(year) ? DAYS_PER_LEAP_YEAR : DAYS_PER_YEAR;
    }

    /**
     * @brief Returns number of days in a month.
     *
     * @param year Year number.
     * @param month Month in range `[1, 12]`.
     *
     * @return Days in the requested month.
     */
    constexpr int days_in_month(int year, int month)
    {
        __ASSERT(month <= MONTHS_PER_YEAR, "Month index out of range");
        __ASSERT(month > 0, "Month index out of range");

        constexpr std::array<int, MONTHS_PER_YEAR> DAYS_PER_MONTH = {
            31,
            28,
            31,
            30,
            31,
            30,
            31,
            31,
            30,
            31,
            30,
            31,
        };

        if (month == FEBRUARY && is_leap_year(year))
        {    // February in a leap year
            return DAYS_IN_LEAP_FEBRUARY;
        }

        return DAYS_PER_MONTH.at(month - 1);
    }

    /**
     * @brief Time-of-day representation.
     */
    struct Time
    {
        int32_t hours   = 0;
        int32_t minutes = 0;
        int32_t seconds = 0;
    };

    /**
     * @brief Calendar date and time representation.
     */
    struct DateTime : Time
    {
        // some reasonable defaults
        int32_t year  = DEFAULT_YEAR;
        int32_t month = JANUARY;
        int32_t mday  = JANUARY;
        int32_t wday  = DEFAULT_WEEKDAY;
    };

    /**
     * @brief Converts seconds from midnight to `Time`.
     *
     * @param seconds Number of seconds from midnight.
     *
     * @return Converted time value.
     */
    inline Time seconds_to_time(int seconds)
    {
        Time time = {};

        time.hours   = seconds / SECONDS_PER_HOUR;
        time.minutes = (seconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
        time.seconds = seconds % SECONDS_PER_MINUTE;

        return time;
    }

    /**
     * @brief Validates `DateTime` field ranges.
     *
     * @param date_time Date-time value to validate.
     *
     * @return `true` when all fields are in valid range, otherwise `false`.
     */
    inline bool is_date_time_valid(const DateTime& date_time)
    {
        if (date_time.seconds >= SECONDS_PER_MINUTE)
        {
            return false;
        }

        if (date_time.seconds < 0)
        {
            return false;
        }

        if (date_time.minutes >= MINUTES_PER_HOUR)
        {
            return false;
        }

        if (date_time.minutes < 0)
        {
            return false;
        }

        if (date_time.hours >= HOURS_PER_DAY)
        {
            return false;
        }

        if (date_time.hours < 0)
        {
            return false;
        }

        if (date_time.year < 0)
        {
            return false;
        }

        if (date_time.month > MONTHS_PER_YEAR)
        {
            return false;
        }

        if (date_time.month < JANUARY)
        {
            return false;
        }

        if (date_time.mday < FIRST_DAY_OF_MONTH)
        {
            return false;
        }

        if (date_time.mday > days_in_month(date_time.year, date_time.month))
        {
            return false;
        }

        if (date_time.wday > DAYS_PER_WEEK)
        {
            return false;
        }

        if (date_time.wday < MONDAY)
        {
            return false;
        }

        return true;
    }

    /**
     * @brief Normalizes and updates date-time fields after arithmetic changes.
     *
     * @param date_time Date-time value to normalize in-place.
     */
    inline void normalize_date_time(DateTime& date_time)
    {
        auto clamp_to_minimum = [&]()
        {
            date_time.year    = 0;
            date_time.month   = JANUARY;
            date_time.mday    = FIRST_DAY_OF_MONTH;
            date_time.hours   = 0;
            date_time.minutes = 0;
            date_time.seconds = 0;
        };

        while (date_time.seconds >= SECONDS_PER_MINUTE)
        {
            date_time.seconds -= SECONDS_PER_MINUTE;
            date_time.minutes++;
        }

        while (date_time.seconds < 0)
        {
            date_time.seconds += SECONDS_PER_MINUTE;
            date_time.minutes--;
        }

        while (date_time.minutes >= MINUTES_PER_HOUR)
        {
            date_time.minutes -= MINUTES_PER_HOUR;
            date_time.hours++;
        }

        while (date_time.minutes < 0)
        {
            date_time.minutes += MINUTES_PER_HOUR;
            date_time.hours--;
        }

        while (date_time.hours >= HOURS_PER_DAY)
        {
            date_time.hours -= HOURS_PER_DAY;
            date_time.mday++;
        }

        while (date_time.hours < 0)
        {
            date_time.hours += HOURS_PER_DAY;
            date_time.mday--;
        }

        auto fix_month = [&]()
        {
            if (date_time.month > MONTHS_PER_YEAR)
            {
                date_time.month = JANUARY;
                date_time.year++;
            }
            else if (date_time.month < JANUARY)
            {
                if (date_time.year == 0)
                {
                    clamp_to_minimum();
                    return;
                }

                date_time.month = DECEMBER;
                date_time.year--;
            }
        };

        if (date_time.year < 0)
        {
            clamp_to_minimum();
        }

        fix_month();

        while (date_time.mday > days_in_month(date_time.year, date_time.month))
        {
            date_time.mday -= days_in_month(date_time.year, date_time.month);
            date_time.month++;
            fix_month();
        }

        while (date_time.mday < FIRST_DAY_OF_MONTH)
        {
            if (date_time.year == 0 && date_time.month == JANUARY)
            {
                clamp_to_minimum();
                break;
            }

            date_time.month--;
            fix_month();
            date_time.mday += days_in_month(date_time.year, date_time.month);
        }

        auto weekday_from_date = [](int day_of_month, int month, int year)
        {
            if (month == JANUARY || month == FEBRUARY)
            {
                month += MONTHS_PER_YEAR;
                year--;
            }

            const int YEAR_OF_CENTURY    = year % CENTURY_YEARS;
            const int ZERO_BASED_CENTURY = year / CENTURY_YEARS;
            const int ZELLER_WEEKDAY     = day_of_month + (ZELLER_FACTOR * (month + JANUARY)) / ZELLER_DIVISOR +
                                           YEAR_OF_CENTURY + YEAR_OF_CENTURY / LEAP_YEAR_FACTOR +
                                           ZERO_BASED_CENTURY / LEAP_YEAR_FACTOR -
                                           ZELLER_CENTURY_FACTOR * ZERO_BASED_CENTURY;

            constexpr std::array<uint8_t, DAYS_PER_WEEK> SUNDAY_FIRST_DAY_REMAP = {
                SATURDAY,
                SUNDAY,
                MONDAY,
                TUESDAY,
                WEDNESDAY,
                THURSDAY,
                FRIDAY,
            };

            return SUNDAY_FIRST_DAY_REMAP[(ZELLER_WEEKDAY % DAYS_PER_WEEK + DAYS_PER_WEEK) % DAYS_PER_WEEK];
        };

        date_time.wday = weekday_from_date(date_time.mday,
                                           date_time.month,
                                           date_time.year);
    }
}    // namespace zlibs::utils::misc
