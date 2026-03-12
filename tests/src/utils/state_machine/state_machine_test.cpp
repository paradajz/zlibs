/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/state_machine/state_machine.h"

using namespace ::testing;
using namespace zlibs::utils;
using namespace zlibs::utils::state_machine;

namespace
{
    LOG_MODULE_REGISTER(state_machine_test, CONFIG_ZLIBS_LOG_LEVEL);

    struct TestData
    {
        bool should_transition = false;

        bool operator==(const TestData&) const = default;
    };

    struct UnhandledEvent
    {
        uint8_t value = 0;
    };

    class BasicStateMock
    {
        public:
        MOCK_METHOD(bool, enter, ());
        MOCK_METHOD(bool, exit, ());
    };

    using ConditionalTransitionAction = Maybe<Transition<class ErrorState>>;

    class ConditionalStateMock
    {
        public:
        MOCK_METHOD(bool, enter, ());
        MOCK_METHOD(bool, exit, ());
        MOCK_METHOD(ConditionalTransitionAction, handle, (const TestData& test_data));
    };

    /*
     * `StateMachine` owns and default-constructs its states, so test states
     * delegate to fixture-owned mocks through these hooks.
     */
    BasicStateMock*       init_state_mock        = nullptr;
    BasicStateMock*       error_state_mock       = nullptr;
    BasicStateMock*       on_state_mock          = nullptr;
    ConditionalStateMock* conditional_state_mock = nullptr;

    class InitState : public InitStateTag
    {
        public:
        InitState() = default;

        bool enter()
        {
            return init_state_mock->enter();
        }

        bool exit()
        {
            return init_state_mock->exit();
        }
    };

    class ErrorState : public ErrorStateTag
    {
        public:
        ErrorState() = default;

        bool enter()
        {
            return error_state_mock->enter();
        }

        bool exit()
        {
            return error_state_mock->exit();
        }
    };

    class OnState
    {
        public:
        OnState() = default;

        bool enter()
        {
            return on_state_mock->enter();
        }

        bool exit()
        {
            return on_state_mock->exit();
        }
    };

    class InitConditionalTransitionState : public InitStateTag
    {
        public:
        InitConditionalTransitionState() = default;

        bool enter()
        {
            return conditional_state_mock->enter();
        }

        bool exit()
        {
            return conditional_state_mock->exit();
        }

        Maybe<Transition<ErrorState>> handle(const TestData& test_data)
        {
            return conditional_state_mock->handle(test_data);
        }
    };

    class ConditionalTransitionState
    {
        public:
        ConditionalTransitionState() = default;

        bool enter()
        {
            return conditional_state_mock->enter();
        }

        bool exit()
        {
            return conditional_state_mock->exit();
        }

        Maybe<Transition<ErrorState>> handle(const TestData& test_data)
        {
            return conditional_state_mock->handle(test_data);
        }
    };
}    // namespace

class StateMachineTest : public Test
{
    public:
    StateMachineTest()  = default;
    ~StateMachineTest() = default;

    protected:
    StrictMock<BasicStateMock>       _init_state_mock;
    StrictMock<BasicStateMock>       _error_state_mock;
    StrictMock<BasicStateMock>       _on_state_mock;
    StrictMock<ConditionalStateMock> _conditional_state_mock;

    void SetUp() override
    {
        LOG_INF("Running test %s", UnitTest::GetInstance()->current_test_info()->name());
        init_state_mock        = &_init_state_mock;
        error_state_mock       = &_error_state_mock;
        on_state_mock          = &_on_state_mock;
        conditional_state_mock = &_conditional_state_mock;
    }

    void TearDown() override
    {
        LOG_INF("Test %s complete", UnitTest::GetInstance()->current_test_info()->name());
        init_state_mock        = nullptr;
        error_state_mock       = nullptr;
        on_state_mock          = nullptr;
        conditional_state_mock = nullptr;
    }
};

TEST_F(StateMachineTest, InitializationSuccess)
{
    using StateMachineType =
        StateMachine<InitState, ErrorState>;
    StateMachineType test_state_machine;

    EXPECT_CALL(_init_state_mock, enter()).WillOnce(Return(true));

    ASSERT_TRUE(test_state_machine.init());
    ASSERT_TRUE((test_state_machine.is_in_state<InitState>()));
    ASSERT_EQ(StateMachineType::state_index<InitState>(), test_state_machine.current_state_index());
}

TEST_F(StateMachineTest, InitializationFailureEnterErrorStateSuccess)
{
    using StateMachineType =
        StateMachine<InitState, ErrorState>;
    StateMachineType test_state_machine;

    InSequence sequence;
    EXPECT_CALL(_init_state_mock, enter()).WillOnce(Return(false));
    EXPECT_CALL(_error_state_mock, enter()).WillOnce(Return(true));

    ASSERT_FALSE(test_state_machine.init());
    ASSERT_TRUE((test_state_machine.is_in_state<ErrorState>()));
    ASSERT_EQ(StateMachineType::state_index<ErrorState>(), test_state_machine.current_state_index());
}

TEST_F(StateMachineTest, InitializationFailureEnterErrorStateFailure)
{
    using StateMachineType =
        StateMachine<InitState, ErrorState>;
    StateMachineType test_state_machine;

    InSequence sequence;
    EXPECT_CALL(_init_state_mock, enter()).WillOnce(Return(false));
    EXPECT_CALL(_error_state_mock, enter()).WillOnce(Return(false));

    ASSERT_FALSE(test_state_machine.init());
    ASSERT_TRUE((test_state_machine.is_in_state<ErrorState>()));
    ASSERT_EQ(StateMachineType::state_index<ErrorState>(), test_state_machine.current_state_index());
}

TEST_F(StateMachineTest, TransitionBeforeInitializationReturnsFalse)
{
    using StateMachineType =
        StateMachine<InitState, ErrorState, OnState>;
    StateMachineType test_state_machine;

    EXPECT_CALL(_init_state_mock, enter()).Times(0);
    EXPECT_CALL(_init_state_mock, exit()).Times(0);
    EXPECT_CALL(_on_state_mock, enter()).Times(0);
    EXPECT_CALL(_error_state_mock, enter()).Times(0);

    ASSERT_FALSE((test_state_machine.transition_to<OnState>()));
}

TEST_F(StateMachineTest, TransitionToOnStateSuccess)
{
    using StateMachineType =
        StateMachine<InitState, ErrorState, OnState>;
    StateMachineType test_state_machine;

    InSequence sequence;
    EXPECT_CALL(_init_state_mock, enter()).WillOnce(Return(true));
    EXPECT_CALL(_init_state_mock, exit()).WillOnce(Return(true));
    EXPECT_CALL(_on_state_mock, enter()).WillOnce(Return(true));

    ASSERT_TRUE(test_state_machine.init());
    ASSERT_TRUE((test_state_machine.transition_to<OnState>()));
    ASSERT_TRUE((test_state_machine.is_in_state<OnState>()));
    ASSERT_EQ(StateMachineType::state_index<OnState>(), test_state_machine.current_state_index());
}

TEST_F(StateMachineTest, TransitionToOnStateFailureOnEnter)
{
    using StateMachineType =
        StateMachine<InitState, ErrorState, OnState>;
    StateMachineType test_state_machine;

    InSequence sequence;
    EXPECT_CALL(_init_state_mock, enter()).WillOnce(Return(true));
    EXPECT_CALL(_init_state_mock, exit()).WillOnce(Return(true));
    EXPECT_CALL(_on_state_mock, enter()).WillOnce(Return(false));
    EXPECT_CALL(_error_state_mock, enter()).WillOnce(Return(true));

    ASSERT_TRUE(test_state_machine.init());
    ASSERT_FALSE((test_state_machine.transition_to<OnState>()));
    ASSERT_TRUE((test_state_machine.is_in_state<ErrorState>()));
    ASSERT_EQ(StateMachineType::state_index<ErrorState>(), test_state_machine.current_state_index());
}

TEST_F(StateMachineTest, TransitionToOnStateFailureOnExit)
{
    using StateMachineType =
        StateMachine<InitState, ErrorState, OnState>;
    StateMachineType test_state_machine;

    InSequence sequence;
    EXPECT_CALL(_init_state_mock, enter()).WillOnce(Return(true));
    EXPECT_CALL(_init_state_mock, exit()).WillOnce(Return(false));
    EXPECT_CALL(_error_state_mock, enter()).WillOnce(Return(true));

    ASSERT_TRUE(test_state_machine.init());
    ASSERT_FALSE((test_state_machine.transition_to<OnState>()));
    ASSERT_TRUE((test_state_machine.is_in_state<ErrorState>()));
    ASSERT_EQ(StateMachineType::state_index<ErrorState>(), test_state_machine.current_state_index());
}

TEST_F(StateMachineTest, EventBeforeInitializationIsIgnored)
{
    using StateMachineType =
        StateMachine<InitConditionalTransitionState, ErrorState>;
    StateMachineType test_state_machine;

    EXPECT_CALL(_conditional_state_mock, handle(_)).Times(0);

    test_state_machine.handle(TestData{
        .should_transition = true,
    });
}

TEST_F(StateMachineTest, TransitionBasedOnEventCondition)
{
    using StateMachineType =
        StateMachine<InitState, ErrorState, ConditionalTransitionState>;
    StateMachineType test_state_machine;
    TestData         no_transition_data = {
        .should_transition = false,
    };
    TestData transition_data = {
        .should_transition = true,
    };

    InSequence sequence;
    EXPECT_CALL(_init_state_mock, enter()).WillOnce(Return(true));
    EXPECT_CALL(_init_state_mock, exit()).WillOnce(Return(true));
    EXPECT_CALL(_conditional_state_mock, enter()).WillOnce(Return(true));
    EXPECT_CALL(_conditional_state_mock, handle(no_transition_data))
        .WillOnce(Return(ConditionalTransitionAction(Nothing{})));
    EXPECT_CALL(_conditional_state_mock, handle(transition_data))
        .WillOnce(Return(ConditionalTransitionAction(Transition<ErrorState>{})));
    EXPECT_CALL(_conditional_state_mock, exit()).WillOnce(Return(true));
    EXPECT_CALL(_error_state_mock, enter()).WillOnce(Return(true));

    ASSERT_TRUE(test_state_machine.init());
    ASSERT_TRUE((test_state_machine.transition_to<ConditionalTransitionState>()));

    test_state_machine.handle(no_transition_data);
    ASSERT_TRUE((test_state_machine.is_in_state<ConditionalTransitionState>()));

    test_state_machine.handle(transition_data);
    ASSERT_TRUE((test_state_machine.is_in_state<ErrorState>()));
    ASSERT_EQ(StateMachineType::state_index<ErrorState>(), test_state_machine.current_state_index());
}

TEST_F(StateMachineTest, NoTransitionOnUnhandledEvent)
{
    using StateMachineType =
        StateMachine<InitState, ErrorState, ConditionalTransitionState>;
    StateMachineType test_state_machine;

    InSequence sequence;
    EXPECT_CALL(_init_state_mock, enter()).WillOnce(Return(true));
    EXPECT_CALL(_init_state_mock, exit()).WillOnce(Return(true));
    EXPECT_CALL(_conditional_state_mock, enter()).WillOnce(Return(true));

    ASSERT_TRUE(test_state_machine.init());
    ASSERT_TRUE((test_state_machine.transition_to<ConditionalTransitionState>()));

    test_state_machine.handle(UnhandledEvent{});
    ASSERT_TRUE((test_state_machine.is_in_state<ConditionalTransitionState>()));
    ASSERT_EQ(StateMachineType::state_index<ConditionalTransitionState>(), test_state_machine.current_state_index());
}
