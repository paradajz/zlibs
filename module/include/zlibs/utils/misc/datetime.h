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
    constexpr inline int SECONDS_PER_DAY    = 86400;
    constexpr inline int SECONDS_PER_HOUR   = 3600;
    constexpr inline int SECONDS_PER_MINUTE = 60;
    constexpr inline int MINUTES_PER_HOUR   = 60;
    constexpr inline int HOURS_PER_DAY      = 24;
    constexpr inline int MONTHS_PER_YEAR    = 12;
    constexpr inline int DAYS_PER_YEAR      = 365;
    constexpr inline int DAYS_PER_LEAP_YEAR = 366;

    /**
     * @brief Checks if a year is leap according to Gregorian rules.
     *
     * @param year Year number.
     *
     * @return `true` for leap year, otherwise `false`.
     */
    constexpr bool is_leap_year(int year)
    {
        if (year % 4 == 0)
        {
            if (year % 100 == 0)
            {
                if (year % 400 == 0)
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

        constexpr std::array<int, MONTHS_PER_YEAR> DAYS_PER_MONTH = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

        if (month == 2 && is_leap_year(year))
        {    // February in a leap year
            return 29;
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
        int32_t year  = 2025;
        int32_t month = 1;
        int32_t mday  = 1;
        int32_t wday  = 3;
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

        if (date_time.month < 1)
        {
            return false;
        }

        if (date_time.mday < 1)
        {
            return false;
        }

        if (date_time.mday > days_in_month(date_time.year, date_time.month))
        {
            return false;
        }

        if (date_time.wday > 7)
        {
            return false;
        }

        if (date_time.wday < 1)
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
    inline void update_date_time(DateTime& date_time)
    {
        if (date_time.seconds >= SECONDS_PER_MINUTE)
        {
            date_time.seconds = 0;
            date_time.minutes += 1;
        }
        else if (date_time.seconds < 0)
        {
            date_time.seconds = SECONDS_PER_MINUTE - 1;
            date_time.minutes -= 1;
        }

        if (date_time.minutes >= MINUTES_PER_HOUR)
        {
            date_time.minutes = 0;
            date_time.hours += 1;
        }
        else if (date_time.minutes < 0)
        {
            date_time.minutes = MINUTES_PER_HOUR - 1;
            date_time.hours -= 1;
        }

        if (date_time.hours >= HOURS_PER_DAY)
        {
            date_time.hours = 0;
            date_time.mday += 1;
        }
        else if (date_time.hours < 0)
        {
            date_time.hours = HOURS_PER_DAY - 1;
            date_time.mday -= 1;
        }

        auto fix_month = [&]()
        {
            if (date_time.month > MONTHS_PER_YEAR)
            {
                date_time.month = 1;
                date_time.year += 1;
            }
            else if (date_time.month < 1)
            {
                date_time.month = 12;
                date_time.year -= 1;
                date_time.mday = days_in_month(date_time.year, date_time.month);
            }
        };

        if (date_time.year < 0)
        {
            date_time.year = 0;
        }

        fix_month();

        if (date_time.mday > days_in_month(date_time.year, date_time.month))
        {
            date_time.mday = 1;
            date_time.month += 1;
            fix_month();
        }
        else if (date_time.mday < 1)
        {
            date_time.month -= 1;
            fix_month();
            date_time.mday = days_in_month(date_time.year, date_time.month);
        }

        auto wday = [](int mday, int month, int year)
        {
            if (month == 1 || month == 2)
            {
                month += 12;
                year -= 1;
            }

            int k = mday;
            int m = month;
            int d = year % 100;
            int c = year / 100;

            int f = k + (13 * (m + 1)) / 5 + d + d / 4 + c / 4 - 2 * c;

            constexpr std::array<uint8_t, 7> SUNDAY_FIRST_DAY_REMAP = {
                6,
                7,
                1,
                2,
                3,
                4,
                5,
            };

            return SUNDAY_FIRST_DAY_REMAP[(f % 7 + 7) % 7];
        };

        date_time.wday = wday(date_time.mday,
                              date_time.month,
                              date_time.year);
    }
}    // namespace zlibs::utils::misc
