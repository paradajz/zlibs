/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/motor_control/motor_control.h"

#include <array>
#include <climits>
#include <functional>

using namespace ::testing;
using namespace zlibs::utils::motor_control;

namespace
{
    LOG_MODULE_REGISTER(motor_control_test, CONFIG_ZLIBS_LOG_LEVEL);

    constexpr size_t TEST_THREAD_STACK_SIZE = 1024;

    K_THREAD_STACK_DEFINE(setup_thread_stack_1, TEST_THREAD_STACK_SIZE);
    K_THREAD_STACK_DEFINE(setup_thread_stack_2, TEST_THREAD_STACK_SIZE);

    template<typename Predicate>
    bool wait_until(Predicate&& predicate, uint32_t timeout_ms = 2000, uint32_t poll_ms = 5)
    {
        const int64_t DEADLINE = k_uptime_get() + timeout_ms;

        while (k_uptime_get() < DEADLINE)
        {
            if (predicate())
            {
                return true;
            }

            k_msleep(poll_ms);
        }

        return predicate();
    }

    class HwaTest : public Hwa
    {
        public:
        HwaTest()
        {
            k_sem_init(&_enable_release_sem, 0, UINT_MAX);
        }

        std::atomic<bool> init_result            = true;
        std::atomic<bool> enable_result          = true;
        std::atomic<bool> disable_result         = true;
        std::atomic<bool> write_step_high_result = true;
        std::atomic<bool> write_step_low_result  = true;
        std::atomic<bool> toggle_step_result     = true;
        std::atomic<bool> set_direction_result   = true;
        std::atomic<bool> hold_enable            = false;

        std::atomic<uint32_t>  init_calls             = 0;
        std::atomic<uint32_t>  enable_calls           = 0;
        std::atomic<uint32_t>  blocked_enable_calls   = 0;
        std::atomic<uint32_t>  disable_calls          = 0;
        std::atomic<uint32_t>  write_step_true_calls  = 0;
        std::atomic<uint32_t>  write_step_false_calls = 0;
        std::atomic<uint32_t>  toggle_step_calls      = 0;
        std::atomic<uint32_t>  pulse_count            = 0;
        std::atomic<uint32_t>  set_direction_calls    = 0;
        std::atomic<Direction> last_direction         = Direction::Cw;

        bool init() override
        {
            init_calls++;
            return init_result.load();
        }

        bool enable() override
        {
            enable_calls++;

            if (hold_enable.load())
            {
                blocked_enable_calls++;
                k_sem_take(&_enable_release_sem, K_FOREVER);
                blocked_enable_calls--;
            }

            return enable_result.load();
        }

        bool disable() override
        {
            disable_calls++;
            return disable_result.load();
        }

        bool write_step(bool state) override
        {
            const bool RESULT = state ? write_step_high_result.load()
                                      : write_step_low_result.load();

            if (state)
            {
                write_step_true_calls++;

                if (RESULT)
                {
                    pulse_count++;
                }
            }
            else
            {
                write_step_false_calls++;
            }

            return RESULT;
        }

        bool toggle_step() override
        {
            const bool RESULT = toggle_step_result.load();

            toggle_step_calls++;

            if (RESULT)
            {
                pulse_count++;
            }

            return RESULT;
        }

        bool set_direction(Direction direction) override
        {
            set_direction_calls++;
            last_direction = direction;
            return set_direction_result.load();
        }

        void release_enable_calls(uint32_t count = 2)
        {
            for (uint32_t i = 0; i < count; i++)
            {
                k_sem_give(&_enable_release_sem);
            }
        }

        private:
        k_sem _enable_release_sem = {};
    };

    struct SetupThreadContext
    {
        MotorControl* control  = nullptr;
        Movement*     movement = nullptr;
        bool*         result   = nullptr;
        k_sem*        ready    = nullptr;
        k_sem*        start    = nullptr;
        k_sem*        done     = nullptr;
    };

    void setup_thread_entry(void* p1, void*, void*)
    {
        auto* ctx = static_cast<SetupThreadContext*>(p1);

        k_sem_give(ctx->ready);
        k_sem_take(ctx->start, K_FOREVER);

        *ctx->result = ctx->control->setup_movement(0, *ctx->movement);

        k_sem_give(ctx->done);
    }
}    // namespace

class MotorControlTest : public Test
{
    public:
    MotorControlTest()  = default;
    ~MotorControlTest() = default;

    protected:
    HwaTest _hwa;
    Config  _config = []()
    {
        Config config            = {};
        config.start_interval_us = 200.0F;
        config.end_interval_us   = 100.0F;
        return config;
    }();

    Motor _motor{ _hwa, _config };

    std::array<std::reference_wrapper<Motor>, 1> _motors = {
        _motor,
    };

    MotorControl _motor_control = MotorControl(_motors);

    Movement make_movement(uint32_t  pulses       = 10,
                           uint32_t  max_run_time = 0,
                           Direction direction    = Direction::Cw,
                           uint32_t  speed        = static_cast<uint32_t>(Config::DEFAULT_END_INTERVAL))
    {
        return Movement{
            .direction    = direction,
            .speed        = speed,
            .pulses       = pulses,
            .max_run_time = max_run_time,
        };
    }

    bool wait_for_state(State state, uint32_t timeout_ms = 2000)
    {
        return wait_until([&]()
                          {
                              return _motor_control.movement_state(0) == state;
                          },
                          timeout_ms);
    }

    static bool wait_for_state(MotorControl& control, size_t motor_index, State state, uint32_t timeout_ms = 2000)
    {
        return wait_until([&]()
                          {
                              return control.movement_state(motor_index) == state;
                          },
                          timeout_ms);
    }

    bool wait_for_pulses(uint32_t count, uint32_t timeout_ms = 1000)
    {
        return wait_until([&]()
                          {
                              return _hwa.pulse_count.load() >= count;
                          },
                          timeout_ms);
    }

    static bool wait_for_pulses(HwaTest& hwa, uint32_t count, uint32_t timeout_ms = 1000)
    {
        return wait_until([&]()
                          {
                              return hwa.pulse_count.load() >= count;
                          },
                          timeout_ms);
    }

    void register_stop_counter(std::atomic<uint32_t>& stop_cb_calls)
    {
        ASSERT_TRUE(_motor_control.register_stop_callback(0, [&]()
                                                          {
                                                              stop_cb_calls++;
                                                          }));
    }

    static void expect_stop_cleanup(std::atomic<uint32_t>& stop_cb_calls,
                                    HwaTest&               hwa,
                                    uint32_t               timeout_ms = 1000)
    {
        ASSERT_TRUE(wait_until([&]()
                               {
                                   return stop_cb_calls.load() == 1 && hwa.disable_calls.load() == 1;
                               },
                               timeout_ms));
    }

    void SetUp() override
    {
        LOG_INF("Running test %s", UnitTest::GetInstance()->current_test_info()->name());
    }

    void TearDown() override
    {
        LOG_INF("Test %s complete", UnitTest::GetInstance()->current_test_info()->name());
    }
};

TEST_F(MotorControlTest, InitializationFailurePropagatesHardwareFailure)
{
    _hwa.init_result = false;

    ASSERT_FALSE(_motor_control.init());
    ASSERT_EQ(1U, _hwa.init_calls.load());
}

TEST_F(MotorControlTest, InitCallbackCanRejectMovementSetup)
{
    std::atomic<uint32_t> init_cb_calls = 0;
    Movement              movement      = make_movement(4);

    ASSERT_TRUE(_motor_control.init());

    _motor_control.register_init_callback(0, [&](const Movement&)
                                          {
                                              init_cb_calls++;
                                              return false;
                                          });

    ASSERT_FALSE(_motor_control.setup_movement(0, movement));
    ASSERT_EQ(State::Stopped, _motor_control.movement_state(0));
    ASSERT_EQ(1U, init_cb_calls.load());
    ASSERT_EQ(0U, _hwa.enable_calls.load());
    ASSERT_EQ(0U, _hwa.disable_calls.load());
}

TEST_F(MotorControlTest, DirectionFailureDisablesDriverAndRejectsMovement)
{
    std::atomic<uint32_t> stop_cb_calls = 0;
    Movement              movement      = make_movement(4);

    ASSERT_TRUE(_motor_control.init());
    register_stop_counter(stop_cb_calls);

    _hwa.set_direction_result = false;

    ASSERT_FALSE(_motor_control.setup_movement(0, movement));
    ASSERT_EQ(State::Stopped, _motor_control.movement_state(0));
    ASSERT_EQ(1U, _hwa.enable_calls.load());
    ASSERT_EQ(1U, _hwa.disable_calls.load());
    ASSERT_EQ(1U, _hwa.set_direction_calls.load());
    ASSERT_EQ(0U, stop_cb_calls.load());
}

TEST_F(MotorControlTest, GracefulStopTransitionsThroughStoppingAndCallsStopCallbackOnce)
{
    std::atomic<uint32_t> stop_cb_calls = 0;
    Movement              movement      = make_movement(20);

    ASSERT_TRUE(_motor_control.init());
    register_stop_counter(stop_cb_calls);

    ASSERT_TRUE(_motor_control.setup_movement(0, movement));
    ASSERT_TRUE(wait_for_pulses(1));
    ASSERT_TRUE(_motor_control.stop_movement(0));
    ASSERT_EQ(State::Stopping, _motor_control.movement_state(0));
    ASSERT_FALSE(_motor_control.stop_movement(0));
    ASSERT_TRUE(wait_for_state(State::Stopped, 2000));
    expect_stop_cleanup(stop_cb_calls, _hwa);
    ASSERT_LT(_hwa.pulse_count.load(), movement.pulses);
}

TEST_F(MotorControlTest, ForceStopStopsMovementAndPreventsFurtherPulses)
{
    std::atomic<uint32_t> stop_cb_calls = 0;
    Movement              movement      = make_movement(20);

    ASSERT_TRUE(_motor_control.init());
    register_stop_counter(stop_cb_calls);

    ASSERT_TRUE(_motor_control.setup_movement(0, movement));
    ASSERT_TRUE(wait_for_pulses(1));

    ASSERT_TRUE(_motor_control.stop_movement(0, true));
    ASSERT_EQ(State::Stopped, _motor_control.movement_state(0));
    expect_stop_cleanup(stop_cb_calls, _hwa);

    const uint32_t PULSE_COUNT_AFTER_STOP = _hwa.pulse_count.load();
    k_msleep(250);
    ASSERT_EQ(PULSE_COUNT_AFTER_STOP, _hwa.pulse_count.load());
}

TEST_F(MotorControlTest, ContinuousMovementStopsOnRunTimeLimit)
{
    std::atomic<uint32_t> stop_cb_calls = 0;
    Movement              movement      = make_movement(0, 250);

    ASSERT_TRUE(_motor_control.init());
    register_stop_counter(stop_cb_calls);

    ASSERT_TRUE(_motor_control.setup_movement(0, movement));
    ASSERT_TRUE(wait_for_pulses(1));
    ASSERT_TRUE(wait_for_state(State::Stopped, 1500));
    expect_stop_cleanup(stop_cb_calls, _hwa);
    ASSERT_GT(_hwa.pulse_count.load(), 0U);
}

TEST_F(MotorControlTest, FailedSetupDoesNotCancelPendingResetCleanup)
{
    std::atomic<uint32_t> stop_cb_calls  = 0;
    Movement              first_movement = make_movement(1);
    Movement              invalid_speed  = make_movement(4, 0, Direction::Cw, 50);

    ASSERT_TRUE(_motor_control.init());
    register_stop_counter(stop_cb_calls);

    ASSERT_TRUE(_motor_control.setup_movement(0, first_movement));
    ASSERT_TRUE(wait_for_state(State::Stopped, 1500));

    _motor_control.register_init_callback(0, [&](const Movement&)
                                          {
                                              k_msleep(50);
                                              return true;
                                          });

    ASSERT_FALSE(_motor_control.setup_movement(0, invalid_speed));
    expect_stop_cleanup(stop_cb_calls, _hwa);
}

TEST_F(MotorControlTest, StepWriteFailureStopsMovementAndTriggersCleanup)
{
    std::atomic<uint32_t> stop_cb_calls = 0;
    Movement              movement      = make_movement(4);

    ASSERT_TRUE(_motor_control.init());
    register_stop_counter(stop_cb_calls);

    _hwa.write_step_high_result = false;

    ASSERT_TRUE(_motor_control.setup_movement(0, movement));
    ASSERT_TRUE(wait_for_state(State::Stopped, 1500));
    expect_stop_cleanup(stop_cb_calls, _hwa);

    ASSERT_EQ(1U, _hwa.write_step_true_calls.load());
    ASSERT_EQ(0U, _hwa.write_step_false_calls.load());
    ASSERT_EQ(0U, _hwa.pulse_count.load());
}

TEST_F(MotorControlTest, ToggleStepModeUsesTogglePathAndResetsStepLowOnce)
{
    HwaTest toggle_hwa;
    Config  toggle_config         = _config;
    toggle_config.use_step_toggle = true;

    Motor toggle_motor(toggle_hwa, toggle_config);

    std::array<std::reference_wrapper<Motor>, 1> motors = {
        toggle_motor,
    };

    MotorControl control(motors);
    Movement     movement = make_movement(4);

    ASSERT_TRUE(control.init());
    ASSERT_TRUE(control.setup_movement(0, movement));
    ASSERT_TRUE(wait_for_state(control, 0, State::Stopped, 1500));
    ASSERT_TRUE(wait_until([&]()
                           {
                               return toggle_hwa.disable_calls.load() == 1;
                           },
                           1000));

    ASSERT_EQ(movement.pulses, toggle_hwa.toggle_step_calls.load());
    ASSERT_EQ(0U, toggle_hwa.write_step_true_calls.load());
    ASSERT_EQ(1U, toggle_hwa.write_step_false_calls.load());
}

TEST_F(MotorControlTest, TogglePulseFailureStopsMovementAndTriggersCleanup)
{
    HwaTest toggle_hwa;
    Config  toggle_config         = _config;
    toggle_config.use_step_toggle = true;
    toggle_hwa.toggle_step_result = false;

    Motor toggle_motor(toggle_hwa, toggle_config);

    std::array<std::reference_wrapper<Motor>, 1> motors = {
        toggle_motor,
    };

    MotorControl          control(motors);
    std::atomic<uint32_t> stop_cb_calls = 0;
    Movement              movement      = make_movement(4);

    ASSERT_TRUE(control.init());

    ASSERT_TRUE(control.register_stop_callback(0, [&]()
                                               {
                                                   stop_cb_calls++;
                                               }));

    ASSERT_TRUE(control.setup_movement(0, movement));
    ASSERT_TRUE(wait_for_state(control, 0, State::Stopped, 1500));
    expect_stop_cleanup(stop_cb_calls, toggle_hwa);

    ASSERT_EQ(1U, toggle_hwa.toggle_step_calls.load());
    ASSERT_EQ(0U, toggle_hwa.pulse_count.load());
    ASSERT_EQ(1U, toggle_hwa.write_step_false_calls.load());
}

TEST_F(MotorControlTest, ReplacingStopCallbackWhileMovingIsRejected)
{
    std::atomic<uint32_t> first_stop_cb_calls  = 0;
    std::atomic<uint32_t> second_stop_cb_calls = 0;
    Movement              movement             = make_movement(20);

    ASSERT_TRUE(_motor_control.init());

    ASSERT_TRUE(_motor_control.register_stop_callback(0, [&]()
                                                      {
                                                          first_stop_cb_calls++;
                                                      }));

    ASSERT_TRUE(_motor_control.setup_movement(0, movement));
    ASSERT_TRUE(wait_for_pulses(1));

    ASSERT_FALSE(_motor_control.register_stop_callback(0, [&]()
                                                       {
                                                           second_stop_cb_calls++;
                                                       }));

    ASSERT_TRUE(_motor_control.stop_movement(0, true));
    ASSERT_TRUE(wait_until([&]()
                           {
                               return first_stop_cb_calls.load() == 1;
                           },
                           1000));
    ASSERT_EQ(0U, second_stop_cb_calls.load());
}

TEST_F(MotorControlTest, OneMotorStoppingDoesNotInterruptAnotherMotor)
{
    HwaTest first_hwa;
    HwaTest second_hwa;
    Config  first_config;
    Config  second_config;

    first_config.start_interval_us  = 200.0F;
    first_config.end_interval_us    = 100.0F;
    second_config.start_interval_us = 200.0F;
    second_config.end_interval_us   = 100.0F;

    Motor first_motor(first_hwa, first_config);
    Motor second_motor(second_hwa, second_config);

    std::array<std::reference_wrapper<Motor>, 2> motors = {
        first_motor,
        second_motor,
    };

    MotorControl control(motors);
    Movement     long_movement  = make_movement(6);
    Movement     short_movement = make_movement(2, 0, Direction::Ccw);

    ASSERT_TRUE(control.init());
    ASSERT_TRUE(control.setup_movement(0, long_movement));
    ASSERT_TRUE(control.setup_movement(1, short_movement));
    ASSERT_TRUE(wait_for_state(control, 1, State::Stopped, 1000));
    ASSERT_NE(State::Stopped, control.movement_state(0));
    ASSERT_TRUE(wait_for_state(control, 0, State::Stopped, 1500));
    ASSERT_GT(first_hwa.pulse_count.load(), second_hwa.pulse_count.load());
}

TEST_F(MotorControlTest, ConcurrentSetupRequestsOnlyStartOneMovement)
{
    Movement first_movement  = make_movement(20);
    Movement second_movement = make_movement(20, 0, Direction::Ccw);

    ASSERT_TRUE(_motor_control.init());

    _hwa.hold_enable = true;

    bool               first_result   = false;
    bool               second_result  = false;
    k_sem              ready_sem      = {};
    k_sem              start_sem      = {};
    k_sem              done_sem       = {};
    k_thread           first_thread   = {};
    k_thread           second_thread  = {};
    SetupThreadContext first_context  = {};
    SetupThreadContext second_context = {};

    k_sem_init(&ready_sem, 0, 2);
    k_sem_init(&start_sem, 0, 2);
    k_sem_init(&done_sem, 0, 2);

    first_context.control  = &_motor_control;
    first_context.movement = &first_movement;
    first_context.result   = &first_result;
    first_context.ready    = &ready_sem;
    first_context.start    = &start_sem;
    first_context.done     = &done_sem;

    second_context.control  = &_motor_control;
    second_context.movement = &second_movement;
    second_context.result   = &second_result;
    second_context.ready    = &ready_sem;
    second_context.start    = &start_sem;
    second_context.done     = &done_sem;

    k_tid_t first_tid = k_thread_create(&first_thread,
                                        setup_thread_stack_1,
                                        K_THREAD_STACK_SIZEOF(setup_thread_stack_1),
                                        setup_thread_entry,
                                        &first_context,
                                        nullptr,
                                        nullptr,
                                        0,
                                        0,
                                        K_NO_WAIT);

    k_tid_t second_tid = k_thread_create(&second_thread,
                                         setup_thread_stack_2,
                                         K_THREAD_STACK_SIZEOF(setup_thread_stack_2),
                                         setup_thread_entry,
                                         &second_context,
                                         nullptr,
                                         nullptr,
                                         0,
                                         0,
                                         K_NO_WAIT);

    ASSERT_EQ(0, k_sem_take(&ready_sem, K_MSEC(500)));
    ASSERT_EQ(0, k_sem_take(&ready_sem, K_MSEC(500)));

    k_sem_give(&start_sem);
    k_sem_give(&start_sem);

    ASSERT_TRUE(wait_until([&]()
                           {
                               return _hwa.blocked_enable_calls.load() >= 1;
                           },
                           500));

    // Keep the first setup call parked in enable() long enough for a second
    // unsynchronized setup call to reach the same point if the motor does not
    // serialize movement startup.
    k_msleep(50);

    _hwa.hold_enable = false;
    _hwa.release_enable_calls();

    ASSERT_EQ(0, k_sem_take(&done_sem, K_MSEC(1000)));
    ASSERT_EQ(0, k_sem_take(&done_sem, K_MSEC(1000)));
    ASSERT_EQ(0, k_thread_join(first_tid, K_MSEC(1000)));
    ASSERT_EQ(0, k_thread_join(second_tid, K_MSEC(1000)));

    ASSERT_EQ(1U, static_cast<uint32_t>(first_result) + static_cast<uint32_t>(second_result));
    ASSERT_EQ(1U, _hwa.enable_calls.load());
    ASSERT_TRUE(_motor_control.stop_movement(0, true));
    ASSERT_TRUE(wait_until([&]()
                           {
                               return _hwa.disable_calls.load() == 1;
                           },
                           1000));
}
