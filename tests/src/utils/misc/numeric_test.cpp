/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/misc/numeric.h"

using namespace ::testing;
using namespace zlibs::utils;

namespace
{
    LOG_MODULE_REGISTER(misc_numeric_test, CONFIG_ZLIBS_LOG_LEVEL);

    static_assert(misc::constrain(-1, 0, 10) == 0);
    static_assert(misc::constrain(11, 0, 10) == 10);
    static_assert(misc::map_range(5, 0, 10, 0, 100) == 50);
    static_assert(misc::map_range(50, 0, 100, 0, 10) == 5);
    static_assert(misc::map_range(15, 0, 10, 0, 10) == 10);
    static_assert(misc::round_to_nearest(13U, 5U) == 15U);
}    // namespace

class NumericTest : public Test
{
    public:
    NumericTest()  = default;
    ~NumericTest() = default;

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

TEST_F(NumericTest, ConstrainClampsValuesAtBothBounds)
{
    ASSERT_EQ(0, misc::constrain(-5, 0, 10));
    ASSERT_EQ(5, misc::constrain(5, 0, 10));
    ASSERT_EQ(10, misc::constrain(15, 0, 10));
}

TEST_F(NumericTest, MapRangeScalesIntoLargerOutputRange)
{
    ASSERT_EQ(0, misc::map_range(-5, 0, 10, 0, 100));
    ASSERT_EQ(50, misc::map_range(5, 0, 10, 0, 100));
    ASSERT_EQ(100, misc::map_range(15, 0, 10, 0, 100));
}

TEST_F(NumericTest, MapRangeScalesIntoSmallerOutputRange)
{
    ASSERT_EQ(0, misc::map_range(-5, 0, 100, 0, 10));
    ASSERT_EQ(5, misc::map_range(50, 0, 100, 0, 10));
    ASSERT_EQ(10, misc::map_range(150, 0, 100, 0, 10));
}

TEST_F(NumericTest, MapRangeReturnsConstrainedValueWhenRangesMatch)
{
    ASSERT_EQ(0, misc::map_range(-5, 0, 10, 0, 10));
    ASSERT_EQ(6, misc::map_range(6, 0, 10, 0, 10));
    ASSERT_EQ(10, misc::map_range(15, 0, 10, 0, 10));
}

TEST_F(NumericTest, MapRangeHandlesCollapsedInputRange)
{
    ASSERT_EQ(7, misc::map_range(0, 7, 7, 0, 100));
    ASSERT_EQ(7, misc::map_range(42, 7, 7, 0, 100));
}

TEST_F(NumericTest, RoundToNearestUsesClosestMultiple)
{
    ASSERT_EQ(10U, misc::round_to_nearest(12U, 5U));
    ASSERT_EQ(15U, misc::round_to_nearest(13U, 5U));
    ASSERT_EQ(20U, misc::round_to_nearest(15U, 10U));
}
