/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/filters/filters.h"

using namespace ::testing;
using namespace zlibs::utils::filters;

namespace
{
    LOG_MODULE_REGISTER(filters_test, CONFIG_ZLIBS_LOG_LEVEL);

    constexpr uint32_t INACTIVITY_WAIT_MS = 15;
    constexpr uint32_t DEBOUNCE_WAIT_MS   = 15;

    struct DisabledAnalogConfig
    {
        static constexpr uint16_t MIN_RAW_READING_DIFF_SLOW            = 10;
        static constexpr uint16_t K_MIN_RAW_READING_DIFF_FAST          = 3;
        static constexpr uint32_t SLOW_FILTER_ENABLE_AFTER_INACTIVE_MS = 10;
        static constexpr uint16_t MEDIAN_FILTER_NUMBER_OF_READINGS     = 1;
        static constexpr uint16_t EMA_FILTER_SMOOTHING_PERCENTAGE      = 100;
        static constexpr bool     ENABLED                              = false;
    };

    struct ThresholdAnalogConfig
    {
        static constexpr uint16_t MIN_RAW_READING_DIFF_SLOW            = 10;
        static constexpr uint16_t K_MIN_RAW_READING_DIFF_FAST          = 3;
        static constexpr uint32_t SLOW_FILTER_ENABLE_AFTER_INACTIVE_MS = 1000;
        static constexpr uint16_t MEDIAN_FILTER_NUMBER_OF_READINGS     = 1;
        static constexpr uint16_t EMA_FILTER_SMOOTHING_PERCENTAGE      = 100;
        static constexpr bool     ENABLED                              = true;
    };

    struct InactivityAnalogConfig
    {
        static constexpr uint16_t MIN_RAW_READING_DIFF_SLOW            = 10;
        static constexpr uint16_t K_MIN_RAW_READING_DIFF_FAST          = 3;
        static constexpr uint32_t SLOW_FILTER_ENABLE_AFTER_INACTIVE_MS = 10;
        static constexpr uint16_t MEDIAN_FILTER_NUMBER_OF_READINGS     = 1;
        static constexpr uint16_t EMA_FILTER_SMOOTHING_PERCENTAGE      = 100;
        static constexpr bool     ENABLED                              = true;
    };

    struct SmoothedAnalogConfig
    {
        static constexpr uint16_t MIN_RAW_READING_DIFF_SLOW            = 0;
        static constexpr uint16_t K_MIN_RAW_READING_DIFF_FAST          = 0;
        static constexpr uint32_t SLOW_FILTER_ENABLE_AFTER_INACTIVE_MS = 1000;
        static constexpr uint16_t MEDIAN_FILTER_NUMBER_OF_READINGS     = 3;
        static constexpr uint16_t EMA_FILTER_SMOOTHING_PERCENTAGE      = 50;
        static constexpr bool     ENABLED                              = true;
    };
}    // namespace

class FiltersTest : public Test
{
    public:
    FiltersTest()  = default;
    ~FiltersTest() = default;

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

TEST_F(FiltersTest, EmaFilterAppliesConfiguredSmoothingAndReset)
{
    EmaFilter<uint16_t, 50> filter = {};

    ASSERT_EQ(50, filter.value(100));
    ASSERT_EQ(75, filter.value(100));

    filter.reset();

    ASSERT_EQ(25, filter.value(50));
}

TEST_F(FiltersTest, MedianFilterReturnsStableValueOnceWindowIsFull)
{
    MedianFilter<uint16_t, 3> filter = {};

    ASSERT_FALSE(filter.filter_value(5).has_value());
    ASSERT_FALSE(filter.filter_value(100).has_value());

    auto median = filter.filter_value(7);

    ASSERT_TRUE(median.has_value());
    ASSERT_EQ(7, median.value());
    ASSERT_EQ(7, filter.value());

    filter.reset();

    ASSERT_FALSE(filter.filter_value(1).has_value());
    ASSERT_FALSE(filter.filter_value(2).has_value());
    ASSERT_EQ(0, filter.value());

    median = filter.filter_value(9);

    ASSERT_TRUE(median.has_value());
    ASSERT_EQ(2, median.value());
    ASSERT_EQ(2, filter.value());
}

TEST_F(FiltersTest, AnalogFilterBypassesFilteringWhenDisabled)
{
    AnalogFilter<DisabledAnalogConfig, 2> filter = {};

    auto first_channel  = filter.filter_value(0, 42);
    auto second_channel = filter.filter_value(1, 512);

    ASSERT_TRUE(first_channel.has_value());
    ASSERT_TRUE(second_channel.has_value());
    ASSERT_EQ(42, first_channel.value());
    ASSERT_EQ(512, second_channel.value());
}

TEST_F(FiltersTest, AnalogFilterUsesFastThresholdUntilDirectionChanges)
{
    AnalogFilter<ThresholdAnalogConfig, 1> filter = {};

    auto first = filter.filter_value(0, 20);

    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(20, first.value());
    ASSERT_EQ(20, filter.value(0));

    ASSERT_FALSE(filter.filter_value(0, 22).has_value());

    auto fast_threshold_sample = filter.filter_value(0, 23);

    ASSERT_TRUE(fast_threshold_sample.has_value());
    ASSERT_EQ(23, fast_threshold_sample.value());

    ASSERT_FALSE(filter.filter_value(0, 18).has_value());

    auto slow_threshold_sample = filter.filter_value(0, 8);

    ASSERT_TRUE(slow_threshold_sample.has_value());
    ASSERT_EQ(8, slow_threshold_sample.value());
    ASSERT_EQ(8, filter.value(0));
}

TEST_F(FiltersTest, AnalogFilterReEnablesSlowThresholdAfterInactivity)
{
    AnalogFilter<InactivityAnalogConfig, 2> filter = {};

    auto first = filter.filter_value(0, 50);

    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(50, first.value());

    k_msleep(INACTIVITY_WAIT_MS);

    ASSERT_FALSE(filter.filter_value(0, 55).has_value());

    auto second_channel = filter.filter_value(1, 20);

    ASSERT_TRUE(second_channel.has_value());
    ASSERT_EQ(20, second_channel.value());

    auto resumed = filter.filter_value(0, 60);

    ASSERT_TRUE(resumed.has_value());
    ASSERT_EQ(60, resumed.value());
}

TEST_F(FiltersTest, AnalogFilterAppliesMedianAndEmaStages)
{
    AnalogFilter<SmoothedAnalogConfig, 1> filter = {};

    ASSERT_FALSE(filter.filter_value(0, 10).has_value());
    ASSERT_FALSE(filter.filter_value(0, 100).has_value());

    auto first = filter.filter_value(0, 20);

    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(10, first.value());
    ASSERT_EQ(10, filter.value(0));

    ASSERT_FALSE(filter.filter_value(0, 20).has_value());
    ASSERT_FALSE(filter.filter_value(0, 20).has_value());

    auto second = filter.filter_value(0, 20);

    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(15, second.value());
    ASSERT_EQ(15, filter.value(0));
}

TEST_F(FiltersTest, DebounceCanSwitchBetweenImmediateAndDelayedUpdates)
{
    Debounce debounce(0);

    ASSERT_EQ(0, debounce.debounce_time_ms());
    ASSERT_FALSE(debounce.state());

    debounce.update(true);
    ASSERT_TRUE(debounce.state());

    debounce.set_debounce_time_ms(5);
    ASSERT_EQ(5, debounce.debounce_time_ms());

    debounce.update(false);
    ASSERT_TRUE(debounce.state());

    k_msleep(DEBOUNCE_WAIT_MS);

    ASSERT_FALSE(debounce.state());
}

TEST_F(FiltersTest, DebounceKeepsStableStateUntilLastUpdateSettles)
{
    Debounce debounce(5);

    debounce.update(true);
    ASSERT_FALSE(debounce.state());

    k_msleep(DEBOUNCE_WAIT_MS);
    ASSERT_TRUE(debounce.state());

    debounce.update(false);
    ASSERT_TRUE(debounce.state());

    debounce.update(true);
    k_msleep(DEBOUNCE_WAIT_MS);

    ASSERT_TRUE(debounce.state());
}
