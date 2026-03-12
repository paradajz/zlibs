/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/misc/mutex.h"
#include "zlibs/utils/threads/threads.h"

using namespace ::testing;
using namespace zlibs::utils;

namespace
{
    LOG_MODULE_REGISTER(misc_mutex_test, CONFIG_ZLIBS_LOG_LEVEL);

    constexpr int WAIT_TIME_MS = 100;

    using MutexWorkerThread = threads::UserThread<misc::StringLiteral{ "mutex-worker" },
                                                  K_PRIO_PREEMPT(0),
                                                  1024>;
}    // namespace

class MutexTest : public Test
{
    public:
    MutexTest()  = default;
    ~MutexTest() = default;

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

TEST_F(MutexTest, LockBlocksOtherThreadUntilUnlock)
{
    misc::Mutex       mutex;
    std::atomic<bool> entered_critical_section = { false };
    k_sem             ready                    = {};
    k_sem             acquired                 = {};

    k_sem_init(&ready, 0, 1);
    k_sem_init(&acquired, 0, 1);

    mutex.lock();

    {
        MutexWorkerThread thread([&]()
                                 {
                                     k_sem_give(&ready);
                                     mutex.lock();
                                     entered_critical_section.store(true, std::memory_order_release);
                                     k_sem_give(&acquired);
                                     mutex.unlock();
                                 });

        ASSERT_TRUE(thread.run());
        ASSERT_EQ(0, k_sem_take(&ready, K_MSEC(WAIT_TIME_MS)));

        k_msleep(10);

        ASSERT_FALSE(entered_critical_section.load(std::memory_order_acquire));
        ASSERT_NE(0, k_sem_take(&acquired, K_MSEC(10)));

        mutex.unlock();

        ASSERT_EQ(0, k_sem_take(&acquired, K_MSEC(WAIT_TIME_MS)));
        ASSERT_TRUE(entered_critical_section.load(std::memory_order_acquire));
    }
}

TEST_F(MutexTest, LockGuardReleasesMutexAtScopeExit)
{
    misc::Mutex      mutex;
    std::atomic<int> shared_value = { 0 };
    k_sem            acquired     = {};

    k_sem_init(&acquired, 0, 1);

    MutexWorkerThread thread([&]()
                             {
                                 mutex.lock();
                                 shared_value.store(2, std::memory_order_release);
                                 k_sem_give(&acquired);
                                 mutex.unlock();
                             });

    {
        const zlibs::utils::misc::LockGuard lock(mutex);

        shared_value.store(1, std::memory_order_release);

        ASSERT_TRUE(thread.run());
        k_msleep(10);
        ASSERT_EQ(1, shared_value.load(std::memory_order_acquire));
        ASSERT_NE(0, k_sem_take(&acquired, K_MSEC(10)));
    }

    ASSERT_EQ(0, k_sem_take(&acquired, K_MSEC(WAIT_TIME_MS)));
    ASSERT_EQ(2, shared_value.load(std::memory_order_acquire));
}
