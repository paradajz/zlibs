/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/misc/datetime.h"

using namespace ::testing;
using namespace zlibs::utils;

namespace
{
    LOG_MODULE_REGISTER(misc_test, CONFIG_ZLIBS_LOG_LEVEL);

    static_assert(misc::is_leap_year(2000));
    static_assert(!misc::is_leap_year(1900));
    static_assert(misc::days_in_year(2024) == misc::DAYS_PER_LEAP_YEAR);
    static_assert(misc::days_in_year(2025) == misc::DAYS_PER_YEAR);
    static_assert(misc::days_in_month(2024, 2) == 29);
    static_assert(misc::days_in_month(2025, 2) == 28);

    misc::DateTime make_date_time(int year,
                                  int month,
                                  int mday,
                                  int hours,
                                  int minutes,
                                  int seconds,
                                  int wday = 1)
    {
        misc::DateTime time = {};

        time.year    = year;
        time.month   = month;
        time.mday    = mday;
        time.hours   = hours;
        time.minutes = minutes;
        time.seconds = seconds;
        time.wday    = wday;

        return time;
    }
}    // namespace

class DateTimeTest : public Test
{
    public:
    DateTimeTest()  = default;
    ~DateTimeTest() = default;

    protected:
    void SetUp() override
    {
        LOG_INF("Running test %s", UnitTest::GetInstance()->current_test_info()->name());
    }

    void TearDown() override
    {
        LOG_INF("Test %s complete", UnitTest::GetInstance()->current_test_info()->name());
    }
};

TEST_F(DateTimeTest, DateTimeUpdateNoChange)
{
    constexpr int YEAR    = 2024;
    constexpr int MONTH   = 1;
    constexpr int DAY     = 1;
    constexpr int HOUR    = 0;
    constexpr int MINUTES = 0;
    constexpr int SECONDS = 59;

    misc::DateTime time_original = {};
    misc::DateTime time_copy     = {};

    time_original.year    = YEAR;
    time_original.month   = MONTH;
    time_original.mday    = DAY;
    time_original.hours   = HOUR;
    time_original.minutes = MINUTES;
    time_original.seconds = SECONDS;

    memcpy(&time_copy, &time_original, sizeof(misc::DateTime));

    misc::normalize_date_time(time_original);

    ASSERT_EQ(time_original.year, time_copy.year);
    ASSERT_EQ(time_original.month, time_copy.month);
    ASSERT_EQ(time_original.mday, time_copy.mday);
    ASSERT_EQ(time_original.hours, time_copy.hours);
    ASSERT_EQ(time_original.minutes, time_copy.minutes);
    ASSERT_EQ(time_original.seconds, time_copy.seconds);
}

TEST_F(DateTimeTest, DateTimeIncrementMinutesAt60Seconds)
{
    constexpr int YEAR    = 2024;
    constexpr int MONTH   = 1;
    constexpr int DAY     = 1;
    constexpr int HOUR    = 0;
    constexpr int MINUTES = 0;
    constexpr int SECONDS = 59;

    misc::DateTime time = {};

    time.year    = YEAR;
    time.month   = MONTH;
    time.mday    = DAY;
    time.hours   = HOUR;
    time.minutes = MINUTES;
    time.seconds = SECONDS;

    time.seconds = 60;
    misc::normalize_date_time(time);

    ASSERT_EQ(0, time.seconds);
    ASSERT_EQ(MINUTES + 1, time.minutes);
}

TEST_F(DateTimeTest, DateTimeNormalizesMultiMinuteSecondOverflow)
{
    constexpr int YEAR    = 2024;
    constexpr int MONTH   = 1;
    constexpr int DAY     = 1;
    constexpr int HOUR    = 0;
    constexpr int MINUTES = 0;

    misc::DateTime time = {};

    time.year    = YEAR;
    time.month   = MONTH;
    time.mday    = DAY;
    time.hours   = HOUR;
    time.minutes = MINUTES;
    time.seconds = 125;

    misc::normalize_date_time(time);

    ASSERT_EQ(5, time.seconds);
    ASSERT_EQ(MINUTES + 2, time.minutes);
    ASSERT_EQ(HOUR, time.hours);
    ASSERT_EQ(DAY, time.mday);
    ASSERT_EQ(MONTH, time.month);
    ASSERT_EQ(YEAR, time.year);
}

TEST_F(DateTimeTest, DateTimeNormalizesMultiMonthDayOverflow)
{
    misc::DateTime time = {};

    time.year    = 2026;
    time.month   = 1;
    time.mday    = 65;
    time.hours   = 0;
    time.minutes = 0;
    time.seconds = 0;

    misc::normalize_date_time(time);

    ASSERT_EQ(2026, time.year);
    ASSERT_EQ(3, time.month);
    ASSERT_EQ(6, time.mday);
    ASSERT_EQ(0, time.hours);
    ASSERT_EQ(0, time.minutes);
    ASSERT_EQ(0, time.seconds);
}

TEST_F(DateTimeTest, DateTimeClampsUnderflowBeforeYearZero)
{
    misc::DateTime time = {};

    time.year    = 0;
    time.month   = 1;
    time.mday    = -500;
    time.hours   = 0;
    time.minutes = 0;
    time.seconds = 0;

    misc::normalize_date_time(time);

    ASSERT_EQ(0, time.year);
    ASSERT_EQ(1, time.month);
    ASSERT_EQ(1, time.mday);
    ASSERT_EQ(0, time.hours);
    ASSERT_EQ(0, time.minutes);
    ASSERT_EQ(0, time.seconds);
}

TEST_F(DateTimeTest, DateTimeClampsNegativeYearToMinimumTimestamp)
{
    misc::DateTime time = {};

    time.year    = -5;
    time.month   = 8;
    time.mday    = 20;
    time.hours   = 13;
    time.minutes = 42;
    time.seconds = 17;

    misc::normalize_date_time(time);

    ASSERT_EQ(0, time.year);
    ASSERT_EQ(1, time.month);
    ASSERT_EQ(1, time.mday);
    ASSERT_EQ(0, time.hours);
    ASSERT_EQ(0, time.minutes);
    ASSERT_EQ(0, time.seconds);
}

TEST_F(DateTimeTest, DateTimeClampsMonthUnderflowAtYearZeroToMinimumTimestamp)
{
    misc::DateTime time = {};

    time.year    = 0;
    time.month   = 0;
    time.mday    = 10;
    time.hours   = 0;
    time.minutes = 0;
    time.seconds = 0;

    misc::normalize_date_time(time);

    ASSERT_EQ(0, time.year);
    ASSERT_EQ(1, time.month);
    ASSERT_EQ(1, time.mday);
    ASSERT_EQ(0, time.hours);
    ASSERT_EQ(0, time.minutes);
    ASSERT_EQ(0, time.seconds);
}

TEST_F(DateTimeTest, DateTimeDecrementMinutesAtNegativeSeconds)
{
    constexpr int YEAR    = 2024;
    constexpr int MONTH   = 1;
    constexpr int DAY     = 1;
    constexpr int HOUR    = 0;
    constexpr int MINUTES = 0;
    constexpr int SECONDS = 59;

    misc::DateTime time = {};

    time.year    = YEAR;
    time.month   = MONTH;
    time.mday    = DAY;
    time.hours   = HOUR;
    time.minutes = MINUTES;
    time.seconds = SECONDS;

    time.seconds = -1;
    misc::normalize_date_time(time);

    ASSERT_EQ(59, time.seconds);
    ASSERT_EQ(59, time.minutes);
}

TEST_F(DateTimeTest, DateTimeDecrementEverythingAtNewYear)
{
    constexpr int YEAR    = 2024;
    constexpr int MONTH   = 1;
    constexpr int DAY     = 1;
    constexpr int HOUR    = 0;
    constexpr int MINUTES = 0;
    constexpr int SECONDS = 0;

    misc::DateTime time = {};

    time.year    = YEAR;
    time.month   = MONTH;
    time.mday    = DAY;
    time.hours   = HOUR;
    time.minutes = MINUTES;
    time.seconds = SECONDS;

    time.seconds = -1;
    misc::normalize_date_time(time);

    ASSERT_EQ(59, time.seconds);
    ASSERT_EQ(59, time.minutes);
    ASSERT_EQ(23, time.hours);
    ASSERT_EQ(misc::days_in_month(YEAR, 12), time.mday);
    ASSERT_EQ(12, time.month);
    ASSERT_EQ(YEAR - 1, time.year);
}

TEST_F(DateTimeTest, DateTimeIncrementEverythingAtNewYearsEve)
{
    constexpr int YEAR    = 2024;
    constexpr int MONTH   = 12;
    constexpr int DAY     = 31;
    constexpr int HOUR    = 23;
    constexpr int MINUTES = 59;
    constexpr int SECONDS = 59;

    misc::DateTime time = {};

    time.year    = YEAR;
    time.month   = MONTH;
    time.mday    = DAY;
    time.hours   = HOUR;
    time.minutes = MINUTES;
    time.seconds = SECONDS;

    time.seconds = 60;
    misc::normalize_date_time(time);

    ASSERT_EQ(0, time.seconds);
    ASSERT_EQ(0, time.minutes);
    ASSERT_EQ(0, time.hours);
    ASSERT_EQ(1, time.mday);
    ASSERT_EQ(1, time.month);
    ASSERT_EQ(YEAR + 1, time.year);
}

TEST_F(DateTimeTest, TimeConversion)
{
    constexpr int DAYS          = 30;
    constexpr int MINUTES       = 6;
    constexpr int SECONDS       = 59;
    constexpr int TOTAL_SECONDS = (DAYS * misc::SECONDS_PER_DAY) + (MINUTES * misc::SECONDS_PER_MINUTE) + SECONDS;

    auto time = misc::seconds_to_time(TOTAL_SECONDS);

    ASSERT_EQ(DAYS * misc::HOURS_PER_DAY, time.hours);
    ASSERT_EQ(MINUTES, time.minutes);
    ASSERT_EQ(SECONDS, time.seconds);
}

TEST_F(DateTimeTest, CalendarHelpersHandleLeapYearRules)
{
    ASSERT_TRUE(misc::is_leap_year(2000));
    ASSERT_FALSE(misc::is_leap_year(1900));
    ASSERT_TRUE(misc::is_leap_year(2024));
    ASSERT_FALSE(misc::is_leap_year(2025));

    ASSERT_EQ(misc::DAYS_PER_LEAP_YEAR, misc::days_in_year(2024));
    ASSERT_EQ(misc::DAYS_PER_YEAR, misc::days_in_year(2025));
    ASSERT_EQ(29, misc::days_in_month(2024, 2));
    ASSERT_EQ(28, misc::days_in_month(2025, 2));
    ASSERT_EQ(30, misc::days_in_month(2025, 4));
}

TEST_F(DateTimeTest, DateTimeValidationRejectsOutOfRangeFields)
{
    auto time = make_date_time(2024, 2, 29, 23, 59, 59, 4);

    ASSERT_TRUE(misc::is_date_time_valid(time));

    time.seconds = 60;
    ASSERT_FALSE(misc::is_date_time_valid(time));

    time         = make_date_time(2024, 2, 29, 23, 59, 59, 4);
    time.minutes = 60;
    ASSERT_FALSE(misc::is_date_time_valid(time));

    time       = make_date_time(2024, 2, 29, 23, 59, 59, 4);
    time.hours = 24;
    ASSERT_FALSE(misc::is_date_time_valid(time));

    time = make_date_time(-1, 2, 29, 23, 59, 59, 4);
    ASSERT_FALSE(misc::is_date_time_valid(time));

    time = make_date_time(2024, 13, 29, 23, 59, 59, 4);
    ASSERT_FALSE(misc::is_date_time_valid(time));

    time = make_date_time(2024, 0, 29, 23, 59, 59, 4);
    ASSERT_FALSE(misc::is_date_time_valid(time));

    time = make_date_time(2024, 2, 30, 23, 59, 59, 4);
    ASSERT_FALSE(misc::is_date_time_valid(time));

    time = make_date_time(2024, 2, 29, 23, 59, 59, 0);
    ASSERT_FALSE(misc::is_date_time_valid(time));

    time = make_date_time(2024, 2, 29, 23, 59, 59, 8);
    ASSERT_FALSE(misc::is_date_time_valid(time));
}

TEST_F(DateTimeTest, DateTimeHandlesLeapDayRollover)
{
    auto time = make_date_time(2024, 2, 29, 23, 59, 60);

    misc::normalize_date_time(time);

    ASSERT_EQ(2024, time.year);
    ASSERT_EQ(3, time.month);
    ASSERT_EQ(1, time.mday);
    ASSERT_EQ(0, time.hours);
    ASSERT_EQ(0, time.minutes);
    ASSERT_EQ(0, time.seconds);
    ASSERT_EQ(5, time.wday);
    ASSERT_TRUE(misc::is_date_time_valid(time));
}

TEST_F(DateTimeTest, DateTimeHandlesNonLeapYearMonthUnderflow)
{
    auto time = make_date_time(2023, 3, 1, 0, 0, -1);

    misc::normalize_date_time(time);

    ASSERT_EQ(2023, time.year);
    ASSERT_EQ(2, time.month);
    ASSERT_EQ(28, time.mday);
    ASSERT_EQ(23, time.hours);
    ASSERT_EQ(59, time.minutes);
    ASSERT_EQ(59, time.seconds);
    ASSERT_EQ(2, time.wday);
    ASSERT_TRUE(misc::is_date_time_valid(time));
}

TEST_F(DateTimeTest, DateTimeRecomputesSundayWeekday)
{
    auto time = make_date_time(2024, 1, 7, 12, 0, 0, 1);

    misc::normalize_date_time(time);

    ASSERT_EQ(2024, time.year);
    ASSERT_EQ(1, time.month);
    ASSERT_EQ(7, time.mday);
    ASSERT_EQ(12, time.hours);
    ASSERT_EQ(0, time.minutes);
    ASSERT_EQ(0, time.seconds);
    ASSERT_EQ(7, time.wday);
    ASSERT_TRUE(misc::is_date_time_valid(time));
}
