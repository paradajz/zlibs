/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/signaling/signaling.h"

using namespace ::testing;
using namespace zlibs::utils;
using namespace zlibs::utils::signaling;

namespace
{
    LOG_MODULE_REGISTER(signaling_test, CONFIG_ZLIBS_LOG_LEVEL);
}    // namespace

class SignalingTest : public Test
{
    public:
    SignalingTest()  = default;
    ~SignalingTest() = default;

    protected:
    void SetUp() override
    {
        LOG_INF("Running test %s", UnitTest::GetInstance()->current_test_info()->name());
    }

    void TearDown() override
    {
        LOG_INF("Test %s complete", UnitTest::GetInstance()->current_test_info()->name());
    }

    template<typename Data>
    void publish_and_wait(const Data& data)
    {
        ASSERT_TRUE(signaling::publish(data));
        k_msleep(1);
    }

    void wait_for_async_dispatch()
    {
        k_msleep(1);
    }
};

TEST_F(SignalingTest, Signals)
{
    constexpr size_t    SUB_ID_1              = 0;
    constexpr size_t    SUB_ID_2              = 1;
    std::vector<size_t> subscriber_call_order = {};

    enum class TestEnum : uint8_t
    {
        Val1,
        Val2,
    };

    struct Data
    {
        uint32_t a = 0;
        int      b = -1;
        TestEnum c = {};
    };

    std::vector<Data> received_data = {};

    auto sub1 = signaling::subscribe<Data>([&](const Data& data)
                                           {
                                               LOG_INF("received data as sub1");
                                               subscriber_call_order.push_back(SUB_ID_1);
                                               received_data.push_back(data);
                                           });

    auto sub2 = signaling::subscribe<Data>([&](const Data& data)
                                           {
                                               LOG_INF("received data as sub2");
                                               subscriber_call_order.push_back(SUB_ID_2);
                                               received_data.push_back(data);
                                           });

    Data data = {};
    data.a    = 1243;
    data.b    = -2;
    data.c    = TestEnum::Val2;

    publish_and_wait(data);

    ASSERT_EQ(2, subscriber_call_order.size());
    ASSERT_EQ(2, received_data.size());
    ASSERT_EQ(SUB_ID_1, subscriber_call_order.at(0));
    ASSERT_EQ(SUB_ID_2, subscriber_call_order.at(1));
    ASSERT_EQ(data.a, received_data.at(0).a);
    ASSERT_EQ(data.b, received_data.at(0).b);
    ASSERT_EQ(data.c, received_data.at(0).c);
    ASSERT_EQ(data.a, received_data.at(1).a);
    ASSERT_EQ(data.b, received_data.at(1).b);
    ASSERT_EQ(data.c, received_data.at(1).c);
}

TEST_F(SignalingTest, DerivedSignal)
{
    enum class TestEnum : uint8_t
    {
        Val1,
        Val2,
    };

    struct Data
    {
        uint32_t a = 0;
        int      b = -1;
        TestEnum c = {};
    };

    struct DerivedData
    {
        uint32_t a = 0;
        int      b = -1;

        bool operator==(DerivedData const& other) const
        {
            return a == other.a && b == other.b;
        }
    };

    class Derivation
    {
        public:
        Derivation()
        {
            _sub =
                signaling::observe<Data>()
                    .only_if([](const Data& data)
                             {
                                 return data.c == TestEnum::Val2;
                             })
                    .transform([](const Data& data)
                               {
                                   DerivedData out = {};
                                   out.a           = data.a;
                                   out.b           = data.b;
                                   return out;
                               })
                    .only_on_change()
                    .subscribe([&](const DerivedData& data)
                               {
                                   signaling::publish(data);
                               });
        }

        private:
        Subscription _sub = {};
    };

    Derivation               derivation;
    std::vector<Data>        received_data_original = {};
    std::vector<DerivedData> received_data_derived  = {};

    auto sub_original = signaling::subscribe<Data>([&](const Data& data)
                                                   {
                                                       received_data_original.push_back(data);
                                                   });

    auto sub_derived = signaling::subscribe<DerivedData>([&](const DerivedData& data)
                                                         {
                                                             received_data_derived.push_back(data);
                                                         });

    /*
     * First run: the first subscriber will always receive data. The second
     * subscriber filters on the `c` field, so data is forwarded to the derived
     * signal only when `c` is `Val2`.
     */
    Data data = {};
    data.a    = 1243;
    data.b    = -2;
    data.c    = TestEnum::Val1;

    publish_and_wait(data);

    ASSERT_EQ(1, received_data_original.size());
    ASSERT_EQ(0, received_data_derived.size());

    // The second subscriber should now receive the data and forward it to the derived signal.
    data.c = TestEnum::Val2;

    publish_and_wait(data);

    ASSERT_EQ(2, received_data_original.size());
    ASSERT_EQ(1, received_data_derived.size());

    // The derived signal count should remain the same because the pushed values are unchanged.
    publish_and_wait(data);
    ASSERT_EQ(3, received_data_original.size());
    ASSERT_EQ(1, received_data_derived.size());

    // After changing some data, the derived signal should be pushed again.
    data.b = 100;
    publish_and_wait(data);
    ASSERT_EQ(4, received_data_original.size());
    ASSERT_EQ(2, received_data_derived.size());
}

TEST_F(SignalingTest, ReplaysLastSignalToNewSubscriberAndTracksLatestValue)
{
    struct Data
    {
        uint32_t value = 0;

        bool operator==(const Data&) const = default;
    };

    ASSERT_FALSE(signaling::last_signal_data<Data>().has_value());

    Data first = {
        .value = 42,
    };

    publish_and_wait(first);

    auto last_signal = signaling::last_signal_data<Data>();
    ASSERT_TRUE(last_signal.has_value());
    ASSERT_EQ(first, *last_signal);

    std::vector<Data> replayed_data = {};

    auto sub = signaling::subscribe<Data>(
        [&](const Data& data)
        {
            replayed_data.push_back(data);
        },
        true);

    wait_for_async_dispatch();

    ASSERT_EQ(1, replayed_data.size());
    ASSERT_EQ(first, replayed_data.at(0));

    Data second = {
        .value = 84,
    };

    publish_and_wait(second);

    ASSERT_EQ(2, replayed_data.size());
    ASSERT_EQ(first, replayed_data.at(0));
    ASSERT_EQ(second, replayed_data.at(1));

    last_signal = signaling::last_signal_data<Data>();
    ASSERT_TRUE(last_signal.has_value());
    ASSERT_EQ(second, *last_signal);
}

TEST_F(SignalingTest, SubscribeCanReportDroppedReplayWhenQueueIsFull)
{
    struct Data
    {
        uint32_t value = 0;

        bool operator==(const Data&) const = default;
    };

    publish_and_wait(Data{ .value = 7 });

    bool publish_failed = false;

    for (uint32_t i = 0; i < CONFIG_ZLIBS_UTILS_SIGNALING_MAX_POOL_SIZE * 4; i++)
    {
        if (!signaling::publish(Data{ .value = i }))
        {
            publish_failed = true;
            break;
        }
    }

    ASSERT_TRUE(publish_failed);

    bool replay_dropped = false;

    auto sub = signaling::subscribe<Data>(
        [](const Data&)
        {
        },
        true,
        &replay_dropped);

    ASSERT_TRUE(replay_dropped);

    wait_for_async_dispatch();
}

TEST_F(SignalingTest, SubscriptionMoveAssignmentTransfersOwnership)
{
    struct Data
    {
        uint8_t value = 0;
    };

    size_t first_subscriber_calls  = 0;
    size_t second_subscriber_calls = 0;

    auto first_subscription = signaling::subscribe<Data>([&](const Data& data)
                                                         {
                                                             first_subscriber_calls += data.value;
                                                         });

    auto second_subscription = signaling::subscribe<Data>([&](const Data& data)
                                                          {
                                                              second_subscriber_calls += data.value;
                                                          });

    publish_and_wait(Data{ .value = 1 });

    ASSERT_EQ(1, first_subscriber_calls);
    ASSERT_EQ(1, second_subscriber_calls);

    second_subscription = std::move(first_subscription);
    first_subscription.reset();

    publish_and_wait(Data{ .value = 1 });

    ASSERT_EQ(2, first_subscriber_calls);
    ASSERT_EQ(1, second_subscriber_calls);

    second_subscription.reset();

    publish_and_wait(Data{ .value = 1 });

    ASSERT_EQ(2, first_subscriber_calls);
    ASSERT_EQ(1, second_subscriber_calls);
}

TEST_F(SignalingTest, AccumulateMaintainsIndependentStatePerSubscription)
{
    struct Data
    {
        int value = 0;
    };

    auto accumulated_signal = signaling::observe<Data>(true).accumulate(
        0,
        [](int state, const Data& data)
        {
            return state + data.value;
        });

    std::vector<int> first_subscriber_values  = {};
    std::vector<int> second_subscriber_values = {};

    auto first_subscription = accumulated_signal.subscribe([&](const int& value)
                                                           {
                                                               first_subscriber_values.push_back(value);
                                                           });

    publish_and_wait(Data{ .value = 1 });
    publish_and_wait(Data{ .value = 2 });

    ASSERT_EQ((std::vector<int>{ 1, 3 }), first_subscriber_values);
    ASSERT_TRUE(second_subscriber_values.empty());

    auto second_subscription = accumulated_signal.subscribe([&](const int& value)
                                                            {
                                                                second_subscriber_values.push_back(value);
                                                            });

    wait_for_async_dispatch();

    ASSERT_EQ((std::vector<int>{ 2 }), second_subscriber_values);

    publish_and_wait(Data{ .value = 3 });

    ASSERT_EQ((std::vector<int>{ 1, 3, 6 }), first_subscriber_values);
    ASSERT_EQ((std::vector<int>{ 2, 5 }), second_subscriber_values);
}
