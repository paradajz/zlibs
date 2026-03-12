/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/misc/bit.h"

using namespace ::testing;
using namespace zlibs::utils;

namespace
{
    LOG_MODULE_REGISTER(misc_bit_test, CONFIG_ZLIBS_LOG_LEVEL);

    constexpr auto COMPILETIME_BIT_TEST = []()
    {
        uint8_t value = 0;

        misc::bit_set(value, 1);
        misc::bit_set(value, 3);
        misc::bit_clear(value, 1);
        misc::bit_write(value, 0, true);
        misc::bit_write(value, 3, false);

        return value;
    }();

    static_assert(COMPILETIME_BIT_TEST == 0x01);
    static_assert(misc::bit_read<uint8_t>(0x80, 7));
    static_assert(!misc::bit_read<uint8_t>(0x80, 6));
}    // namespace

class BitTest : public Test
{
    public:
    BitTest()  = default;
    ~BitTest() = default;

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

TEST_F(BitTest, BitReadReportsSetAndClearedBits)
{
    constexpr uint32_t VALUE = 0b10100100;

    ASSERT_TRUE(misc::bit_read(VALUE, 7));
    ASSERT_FALSE(misc::bit_read(VALUE, 6));
    ASSERT_TRUE(misc::bit_read(VALUE, 5));
    ASSERT_FALSE(misc::bit_read(VALUE, 0));
}

TEST_F(BitTest, BitSetEnablesRequestedBitWithoutTouchingOthers)
{
    uint16_t value = 0b00010010;

    misc::bit_set(value, 0);
    misc::bit_set(value, 7);

    ASSERT_EQ(0b10010011, value);
}

TEST_F(BitTest, BitClearDisablesRequestedBitWithoutTouchingOthers)
{
    uint16_t value = 0b11110011;

    misc::bit_clear(value, 0);
    misc::bit_clear(value, 5);

    ASSERT_EQ(0b11010010, value);
}

TEST_F(BitTest, BitWriteSwitchesBitOnAndOff)
{
    uint8_t value = 0;

    misc::bit_write(value, 2, true);
    misc::bit_write(value, 5, true);
    ASSERT_EQ(0b00100100, value);

    misc::bit_write(value, 2, false);
    ASSERT_EQ(0b00100000, value);
    ASSERT_FALSE(misc::bit_read(value, 2));
    ASSERT_TRUE(misc::bit_read(value, 5));
}
