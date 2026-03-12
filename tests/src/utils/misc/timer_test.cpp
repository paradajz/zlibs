/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/misc/timer.h"

using namespace ::testing;
using namespace zlibs::utils;

namespace
{
    LOG_MODULE_REGISTER(misc_timer_test, CONFIG_ZLIBS_LOG_LEVEL);

    constexpr uint32_t TIMER_PERIOD_MS = 20;
    constexpr uint32_t WAIT_TIME_MS    = 250;
    constexpr uint32_t QUIET_TIME_MS   = 80;
}    // namespace

class TimerTest : public Test
{
    public:
    TimerTest()  = default;
    ~TimerTest() = default;

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

TEST_F(TimerTest, OneShotTimerExpiresOnlyOnce)
{
    std::atomic<uint32_t> expirations = { 0 };
    k_sem                 fired       = {};

    k_sem_init(&fired, 0, 4);

    misc::Timer timer(misc::Timer::Type::OneShot, [&]()
                      {
                          expirations.fetch_add(1, std::memory_order_release);
                          k_sem_give(&fired);
                      });

    ASSERT_TRUE(timer.start(TIMER_PERIOD_MS));
    ASSERT_EQ(0, k_sem_take(&fired, K_MSEC(WAIT_TIME_MS)));

    k_msleep(QUIET_TIME_MS);

    EXPECT_EQ(1u, expirations.load(std::memory_order_acquire));
    EXPECT_NE(0, k_sem_take(&fired, K_MSEC(TIMER_PERIOD_MS * 2)));
}

TEST_F(TimerTest, RepeatingTimerExpiresUntilStopped)
{
    std::atomic<uint32_t> expirations = { 0 };
    k_sem                 fired       = {};

    k_sem_init(&fired, 0, 8);

    misc::Timer timer(misc::Timer::Type::Repeating, [&]()
                      {
                          expirations.fetch_add(1, std::memory_order_release);
                          k_sem_give(&fired);
                      });

    ASSERT_TRUE(timer.start(TIMER_PERIOD_MS));
    ASSERT_EQ(0, k_sem_take(&fired, K_MSEC(WAIT_TIME_MS)));
    ASSERT_EQ(0, k_sem_take(&fired, K_MSEC(WAIT_TIME_MS)));

    timer.stop();

    const auto expirations_after_stop = expirations.load(std::memory_order_acquire);

    k_msleep(QUIET_TIME_MS);

    EXPECT_GE(expirations_after_stop, 2u);
    EXPECT_EQ(expirations_after_stop, expirations.load(std::memory_order_acquire));
}
