/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/midi/midi.h"
#include "zlibs/utils/threads/threads.h"

#include <atomic>
#include <deque>
#include <vector>

using namespace ::testing;
using namespace zlibs::utils::midi;

namespace
{
    constexpr int     SEND_THREAD_COUNT          = 2;
    constexpr int     SEND_ITERATIONS_PER_THREAD = 100;
    constexpr int     THREAD_TEST_TIMEOUT_MS     = 1000;
    constexpr uint8_t THREAD_1_CHANNEL           = 1;
    constexpr uint8_t THREAD_2_CHANNEL           = 2;
    constexpr uint8_t THREAD_1_NOTE              = 0x11;
    constexpr uint8_t THREAD_2_NOTE              = 0x33;
    constexpr uint8_t THREAD_1_VELOCITY          = 0x22;
    constexpr uint8_t THREAD_2_VELOCITY          = 0x44;

    using MidiSendThread1 = zlibs::utils::threads::UserThread<zlibs::utils::misc::StringLiteral{ "midi-send-1" },
                                                              K_PRIO_PREEMPT(0),
                                                              1024>;
    using MidiSendThread2 = zlibs::utils::threads::UserThread<zlibs::utils::misc::StringLiteral{ "midi-send-2" },
                                                              K_PRIO_PREEMPT(0),
                                                              1024>;

    class FakeTransport : public Transport
    {
        public:
        bool init() override
        {
            init_count++;
            return init_result;
        }

        bool deinit() override
        {
            deinit_count++;
            return deinit_result;
        }

        bool begin_transmission(MessageType type) override
        {
            begin_types.push_back(type);
            return begin_result;
        }

        bool write(uint8_t data) override
        {
            tx_data.push_back(data);
            return write_result;
        }

        bool end_transmission() override
        {
            end_count++;
            return end_result;
        }

        std::optional<uint8_t> read() override
        {
            if (rx_data.empty())
            {
                return {};
            }

            const auto DATA = rx_data.front();
            rx_data.pop_front();
            return DATA;
        }

        bool init_result   = true;
        bool deinit_result = true;
        bool begin_result  = true;
        bool write_result  = true;
        bool end_result    = true;

        int                      init_count   = 0;
        int                      deinit_count = 0;
        int                      end_count    = 0;
        std::vector<MessageType> begin_types  = {};
        std::vector<uint8_t>     tx_data      = {};
        std::deque<uint8_t>      rx_data      = {};
    };

    class FakeThru : public Thru
    {
        public:
        bool begin_transmission(MessageType type) override
        {
            begin_types.push_back(type);
            return true;
        }

        bool write(uint8_t data) override
        {
            tx_data.push_back(data);
            return true;
        }

        bool end_transmission() override
        {
            end_count++;
            return true;
        }

        std::vector<MessageType> begin_types = {};
        std::vector<uint8_t>     tx_data     = {};
        int                      end_count   = 0;
    };

    class MidiBaseTest : public Test
    {
        protected:
        void SetUp() override
        {
            ASSERT_TRUE(_midi.init());
            _midi.use_recursive_parsing(true);
        }

        void TearDown() override
        {
            ASSERT_TRUE(_midi.deinit());
        }

        FakeTransport _transport;
        Base          _midi = Base(_transport);
    };

    struct SendThreadContext
    {
        Base*             midi       = nullptr;
        uint8_t           note       = 0;
        uint8_t           velocity   = 0;
        uint8_t           channel    = 0;
        int               iterations = 0;
        k_sem*            ready      = nullptr;
        k_sem*            start      = nullptr;
        k_sem*            done       = nullptr;
        std::atomic<int>* sent_count = nullptr;
    };

    void send_worker(void* p1, void*, void*)
    {
        auto* context = static_cast<SendThreadContext*>(p1);

        k_sem_give(context->ready);
        (void)k_sem_take(context->start, K_FOREVER);

        for (int i = 0; i < context->iterations; i++)
        {
            if (context->midi->send_note_on(context->note, context->velocity, context->channel))
            {
                context->sent_count->fetch_add(1, std::memory_order_relaxed);
            }
        }

        k_sem_give(context->done);
    }
}    // namespace

TEST_F(MidiBaseTest, InitAndDeinitCallTransport)
{
    EXPECT_TRUE(_midi.initialized());
    EXPECT_EQ(1, _transport.init_count);

    EXPECT_TRUE(_midi.deinit());
    EXPECT_FALSE(_midi.initialized());
    EXPECT_EQ(1, _transport.deinit_count);

    EXPECT_TRUE(_midi.init());
    EXPECT_TRUE(_midi.initialized());
    EXPECT_EQ(2, _transport.init_count);
}

TEST_F(MidiBaseTest, SendNoteOnWritesStatusAndData)
{
    ASSERT_TRUE(_midi.send_note_on(60, 100, 1));

    ASSERT_EQ(1, _transport.begin_types.size());
    EXPECT_EQ(MessageType::NoteOn, _transport.begin_types[0]);

    const std::vector<uint8_t> EXPECTED = { 0x90, 60, 100 };
    EXPECT_EQ(EXPECTED, _transport.tx_data);
}

TEST_F(MidiBaseTest, SendNoteOnWithRunningStatusSkipsRepeatedStatusByte)
{
    _midi.set_running_status_state(true);

    ASSERT_TRUE(_midi.send_note_on(60, 100, 1));
    ASSERT_TRUE(_midi.send_note_on(61, 101, 1));

    const std::vector<uint8_t> EXPECTED = { 0x90, 60, 100, 61, 101 };
    EXPECT_EQ(EXPECTED, _transport.tx_data);
}

TEST_F(MidiBaseTest, SendRejectsInvalidChannel)
{
    EXPECT_FALSE(_midi.send_note_on(60, 100, 0));
    EXPECT_FALSE(_midi.send_note_on(60, 100, 17));

    EXPECT_TRUE(_transport.begin_types.empty());
    EXPECT_TRUE(_transport.tx_data.empty());
}

TEST_F(MidiBaseTest, Send14BitControlChangeWritesCoarseAndFineControllers)
{
    ASSERT_TRUE(_midi.send_control_change_14bit(10, 0x1234, 2));

    const std::vector<uint8_t> EXPECTED = {
        0xB1,
        10,
        0x24,
        0xB1,
        42,
        0x34,
    };

    EXPECT_EQ(EXPECTED, _transport.tx_data);
}

TEST_F(MidiBaseTest, ReadParsesChannelMessage)
{
    _transport.rx_data = { 0x91, 0x40, 0x7F };

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(2, _midi.message().channel);
    EXPECT_EQ(0x40, _midi.message().data1);
    EXPECT_EQ(0x7F, _midi.message().data2);
}

TEST_F(MidiBaseTest, ReadParsesRunningStatusMessage)
{
    _transport.rx_data = { 0x92, 0x30, 0x40, 0x31, 0x41 };

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(3, _midi.message().channel);
    EXPECT_EQ(0x30, _midi.message().data1);
    EXPECT_EQ(0x40, _midi.message().data2);

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(3, _midi.message().channel);
    EXPECT_EQ(0x31, _midi.message().data1);
    EXPECT_EQ(0x41, _midi.message().data2);
}

TEST_F(MidiBaseTest, ReadForwardsMessageToRegisteredThruInterface)
{
    FakeThru thru;
    _midi.register_thru_interface(thru);

    _transport.rx_data = { 0x90, 0x3C, 0x64 };

    ASSERT_TRUE(_midi.read());

    ASSERT_EQ(1, thru.begin_types.size());
    EXPECT_EQ(MessageType::NoteOn, thru.begin_types[0]);

    const std::vector<uint8_t> EXPECTED = { 0x90, 0x3C, 0x64 };
    EXPECT_EQ(EXPECTED, thru.tx_data);
    EXPECT_EQ(1, thru.end_count);
}

TEST_F(MidiBaseTest, UnregisterThruStopsForwarding)
{
    FakeThru thru;
    _midi.register_thru_interface(thru);
    _midi.unregister_thru_interface(thru);

    _transport.rx_data = { 0x90, 0x3C, 0x64 };

    ASSERT_TRUE(_midi.read());
    EXPECT_TRUE(thru.begin_types.empty());
    EXPECT_TRUE(thru.tx_data.empty());
    EXPECT_EQ(0, thru.end_count);
}

TEST_F(MidiBaseTest, ConcurrentSendDoesNotInterleaveMessageBytes)
{
    _midi.set_running_status_state(false);

    k_sem ready = {};
    k_sem start = {};
    k_sem done  = {};

    k_sem_init(&ready, 0, SEND_THREAD_COUNT);
    k_sem_init(&start, 0, SEND_THREAD_COUNT);
    k_sem_init(&done, 0, SEND_THREAD_COUNT);

    std::atomic<int> sent_count_thread_1 = { 0 };
    std::atomic<int> sent_count_thread_2 = { 0 };

    SendThreadContext context_1 = {
        .midi       = &_midi,
        .note       = THREAD_1_NOTE,
        .velocity   = THREAD_1_VELOCITY,
        .channel    = THREAD_1_CHANNEL,
        .iterations = SEND_ITERATIONS_PER_THREAD,
        .ready      = &ready,
        .start      = &start,
        .done       = &done,
        .sent_count = &sent_count_thread_1,
    };

    SendThreadContext context_2 = {
        .midi       = &_midi,
        .note       = THREAD_2_NOTE,
        .velocity   = THREAD_2_VELOCITY,
        .channel    = THREAD_2_CHANNEL,
        .iterations = SEND_ITERATIONS_PER_THREAD,
        .ready      = &ready,
        .start      = &start,
        .done       = &done,
        .sent_count = &sent_count_thread_2,
    };

    {
        MidiSendThread1 thread_1([&]()
                                 {
                                     send_worker(&context_1, nullptr, nullptr);
                                 });

        MidiSendThread2 thread_2([&]()
                                 {
                                     send_worker(&context_2, nullptr, nullptr);
                                 });

        ASSERT_TRUE(thread_1.run());
        ASSERT_TRUE(thread_2.run());

        ASSERT_EQ(0, k_sem_take(&ready, K_MSEC(THREAD_TEST_TIMEOUT_MS)));
        ASSERT_EQ(0, k_sem_take(&ready, K_MSEC(THREAD_TEST_TIMEOUT_MS)));

        k_sem_give(&start);
        k_sem_give(&start);

        ASSERT_EQ(0, k_sem_take(&done, K_MSEC(THREAD_TEST_TIMEOUT_MS)));
        ASSERT_EQ(0, k_sem_take(&done, K_MSEC(THREAD_TEST_TIMEOUT_MS)));
    }

    EXPECT_EQ(SEND_ITERATIONS_PER_THREAD, sent_count_thread_1.load(std::memory_order_relaxed));
    EXPECT_EQ(SEND_ITERATIONS_PER_THREAD, sent_count_thread_2.load(std::memory_order_relaxed));

    constexpr size_t BYTES_PER_NOTE_ON = 3;
    const size_t     EXPECTED_MESSAGES = SEND_THREAD_COUNT * SEND_ITERATIONS_PER_THREAD;
    const size_t     EXPECTED_BYTES    = EXPECTED_MESSAGES * BYTES_PER_NOTE_ON;

    ASSERT_EQ(EXPECTED_BYTES, _transport.tx_data.size());
    ASSERT_EQ(0, _transport.tx_data.size() % BYTES_PER_NOTE_ON);

    size_t thread_1_messages = 0;
    size_t thread_2_messages = 0;

    for (size_t i = 0; i < _transport.tx_data.size(); i += BYTES_PER_NOTE_ON)
    {
        const uint8_t STATUS = _transport.tx_data[i + 0];
        const uint8_t DATA1  = _transport.tx_data[i + 1];
        const uint8_t DATA2  = _transport.tx_data[i + 2];

        if (STATUS == 0x90)
        {
            EXPECT_EQ(THREAD_1_NOTE, DATA1);
            EXPECT_EQ(THREAD_1_VELOCITY, DATA2);
            thread_1_messages++;
        }
        else if (STATUS == 0x91)
        {
            EXPECT_EQ(THREAD_2_NOTE, DATA1);
            EXPECT_EQ(THREAD_2_VELOCITY, DATA2);
            thread_2_messages++;
        }
        else
        {
            ADD_FAILURE() << "Unexpected STATUS byte: " << static_cast<int>(STATUS);
        }
    }

    EXPECT_EQ(SEND_ITERATIONS_PER_THREAD, thread_1_messages);
    EXPECT_EQ(SEND_ITERATIONS_PER_THREAD, thread_2_messages);
}
