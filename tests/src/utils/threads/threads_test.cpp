/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/misc/kwork_delayable.h"
#include "zlibs/utils/threads/threads.h"

#include <array>
#include <memory>
#include <string_view>

LOG_MODULE_REGISTER(threads_test);

using namespace zlibs::utils;
using namespace ::testing;

namespace
{
    constexpr int WAIT_TIME_MS = 250;

    using ThreadNameMetadata = threads::BaseThread<misc::StringLiteral{ "named-thread" }, 1024>;
    using NamedUserThread    = threads::UserThread<misc::StringLiteral{ "named-thread" }, K_PRIO_PREEMPT(0), 1024>;
    using EmptyUserThread    = threads::UserThread<misc::StringLiteral{ "empty-thread" }, K_PRIO_PREEMPT(0), 1024>;
    using LoopingUserThread  = threads::UserThread<misc::StringLiteral{ "looping-thread" }, K_PRIO_PREEMPT(0), 1024>;
    using BlockingUserThread = threads::UserThread<misc::StringLiteral{ "blocking-thread" }, K_PRIO_PREEMPT(0), 1024>;
    using TestWorkqueue      = threads::WorkqueueThread<misc::StringLiteral{ "testing-workqueue" }, K_PRIO_PREEMPT(0), 1024>;
}    // namespace

class ThreadsTest : public Test
{
    public:
    ThreadsTest()  = default;
    ~ThreadsTest() = default;

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

TEST_F(ThreadsTest, UserThreadExposesMetadataAndRuntimeName)
{
    constexpr std::string_view EXPECTED_NAME = "named-thread";

    k_sem executed = {};
    k_sem_init(&executed, 0, 1);

    std::array<char, 32> runtime_name     = {};
    int                  name_copy_result = -1;

    ASSERT_EQ(EXPECTED_NAME, ThreadNameMetadata::name());

    {
        NamedUserThread thread([&]()
                               {
                                   name_copy_result = k_thread_name_copy(k_current_get(), runtime_name.data(), runtime_name.size());
                                   k_sem_give(&executed);
                               });

        ASSERT_TRUE(thread.run());
        ASSERT_EQ(0, k_sem_take(&executed, K_MSEC(WAIT_TIME_MS)));
    }

    ASSERT_EQ(0, name_copy_result);
    ASSERT_STREQ(EXPECTED_NAME.data(), runtime_name.data());
}

TEST_F(ThreadsTest, UserThreadRunReturnsFalseForEmptyCallback)
{
    EmptyUserThread thread(EmptyUserThread::ThreadCallback{});

    ASSERT_FALSE(thread.run());
}

TEST_F(ThreadsTest, UserThreadDestructorInvokesExitCallback)
{
    k_sem first_iteration = {};
    k_sem_init(&first_iteration, 0, 1);

    std::atomic<bool> keep_running = { true };
    std::atomic<int>  iterations   = { 0 };
    std::atomic<int>  exit_calls   = { 0 };

    {
        LoopingUserThread thread([&]()
                                 {
                                     if (iterations.fetch_add(1, std::memory_order_relaxed) == 0)
                                     {
                                         k_sem_give(&first_iteration);
                                     }

                                     while (keep_running.load(std::memory_order_acquire))
                                     {
                                         iterations.fetch_add(1, std::memory_order_relaxed);
                                         k_msleep(1);
                                     }
                                 },
                                 [&]()
                                 {
                                     exit_calls.fetch_add(1, std::memory_order_relaxed);
                                     keep_running.store(false, std::memory_order_release);
                                 });

        ASSERT_TRUE(thread.run());
        ASSERT_EQ(0, k_sem_take(&first_iteration, K_MSEC(WAIT_TIME_MS)));
    }

    ASSERT_GT(iterations.load(std::memory_order_relaxed), 0);
    ASSERT_EQ(1, exit_calls.load(std::memory_order_relaxed));
}

TEST_F(ThreadsTest, UserThreadDestructorAbortsBlockingThread)
{
    k_sem entered_thread = {};
    k_sem_init(&entered_thread, 0, 1);

    std::atomic<int> exit_calls = { 0 };

    auto thread = std::make_unique<BlockingUserThread>([&]()
                                                       {
                                                           k_sem_give(&entered_thread);
                                                           k_sleep(K_FOREVER);
                                                       },
                                                       [&]()
                                                       {
                                                           exit_calls.fetch_add(1, std::memory_order_relaxed);
                                                       });

    ASSERT_TRUE(thread->run());
    ASSERT_EQ(0, k_sem_take(&entered_thread, K_MSEC(WAIT_TIME_MS)));

    thread.reset();

    ASSERT_EQ(1, exit_calls.load(std::memory_order_relaxed));
}

TEST_F(ThreadsTest, WorkqueueHandleIsSingletonAndExecutesQueuedWork)
{
    constexpr std::string_view EXPECTED_NAME = "testing-workqueue";

    k_sem first_work_done  = {};
    k_sem second_work_done = {};
    k_sem_init(&first_work_done, 0, 1);
    k_sem_init(&second_work_done, 0, 1);

    std::atomic<int> work_executions = { 0 };

    auto* first_handle  = TestWorkqueue::handle();
    auto* second_handle = TestWorkqueue::handle();

    ASSERT_NE(nullptr, first_handle);
    ASSERT_EQ(first_handle, second_handle);
    ASSERT_EQ(EXPECTED_NAME, TestWorkqueue::BaseThreadType::name());

    misc::KworkDelayable first_work([&]()
                                    {
                                        work_executions.fetch_add(1, std::memory_order_relaxed);
                                        k_sem_give(&first_work_done);
                                    },
                                    first_handle);

    misc::KworkDelayable second_work([&]()
                                     {
                                         work_executions.fetch_add(1, std::memory_order_relaxed);
                                         k_sem_give(&second_work_done);
                                     },
                                     second_handle);

    ASSERT_TRUE(first_work.reschedule(0));
    ASSERT_TRUE(second_work.reschedule(0));

    ASSERT_EQ(0, k_sem_take(&first_work_done, K_MSEC(WAIT_TIME_MS)));
    ASSERT_EQ(0, k_sem_take(&second_work_done, K_MSEC(WAIT_TIME_MS)));
    ASSERT_EQ(2, work_executions.load(std::memory_order_relaxed));
}
