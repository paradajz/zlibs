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
    constexpr uint8_t TEST_GROUP                 = 0;
    constexpr int     SEND_THREAD_COUNT          = 2;
    constexpr int     SEND_ITERATIONS_PER_THREAD = 100;
    constexpr int     THREAD_TEST_TIMEOUT_MS     = 1000;
    constexpr uint8_t THREAD_1_CHANNEL           = 0;
    constexpr uint8_t THREAD_2_CHANNEL           = 1;
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

        bool write([[maybe_unused]] const midi_ump& packet) override
        {
            tx_packets.push_back(packet);
            return write_result;
        }

        std::optional<midi_ump> read() override
        {
            if (rx_packets.empty())
            {
                return {};
            }

            const auto packet = rx_packets.front();
            rx_packets.pop_front();
            return packet;
        }

        bool init_result   = true;
        bool deinit_result = true;
        bool write_result  = true;

        int                   init_count   = 0;
        int                   deinit_count = 0;
        std::vector<midi_ump> tx_packets   = {};
        std::deque<midi_ump>  rx_packets   = {};
    };

    class FakeThru : public Thru
    {
        public:
        bool write(const midi_ump& packet) override
        {
            tx_packets.push_back(packet);
            return true;
        }

        std::vector<midi_ump> tx_packets = {};
    };

    class MidiBaseTest : public Test
    {
        protected:
        void SetUp() override
        {
            ASSERT_TRUE(_midi.init());
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

    void send_worker(SendThreadContext& context)
    {
        k_sem_give(context.ready);
        (void)k_sem_take(context.start, K_FOREVER);

        for (int i = 0; i < context.iterations; i++)
        {
            if (context.midi->send(midi1::note_on(TEST_GROUP, context.channel, context.note, context.velocity)))
            {
                context.sent_count->fetch_add(1, std::memory_order_relaxed);
            }
        }

        k_sem_give(context.done);
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

TEST_F(MidiBaseTest, ThruInterfaceAccessorReturnsUnderlyingTransport)
{
    EXPECT_EQ(static_cast<Thru*>(&_transport), &_midi.thru_interface());
}

TEST_F(MidiBaseTest, ThruInterfaceCanBeRegisteredAndUnregistered)
{
    FakeTransport other_transport;
    Base          other_midi(other_transport);

    _midi.register_thru_interface(other_midi.thru_interface());
    _transport.rx_packets.push_back(midi1_channel_voice_ump(0, 0x90, 0x3C, 0x64));

    ASSERT_TRUE(_midi.read().has_value());
    ASSERT_EQ(1, other_transport.tx_packets.size());

    _midi.unregister_thru_interface(other_midi.thru_interface());
    _transport.rx_packets.push_back(midi1_channel_voice_ump(0, 0x90, 0x3D, 0x65));

    ASSERT_TRUE(_midi.read().has_value());
    ASSERT_EQ(1, other_transport.tx_packets.size());
}

TEST_F(MidiBaseTest, SendNoteOnWritesStatusAndData)
{
    ASSERT_TRUE(_midi.send(midi1::note_on(TEST_GROUP, 0, 60, 100)));

    ASSERT_EQ(1, _transport.tx_packets.size());
    EXPECT_EQ(0x90, UMP_MIDI_STATUS(_transport.tx_packets.at(0)));
    EXPECT_EQ(60, UMP_MIDI1_P1(_transport.tx_packets.at(0)));
    EXPECT_EQ(100, UMP_MIDI1_P2(_transport.tx_packets.at(0)));
}

TEST_F(MidiBaseTest, SendNoteOnWritesOneUmpPerCall)
{
    ASSERT_TRUE(_midi.send(midi1::note_on(TEST_GROUP, 0, 60, 100)));
    ASSERT_TRUE(_midi.send(midi1::note_on(TEST_GROUP, 0, 61, 101)));

    ASSERT_EQ(2, _transport.tx_packets.size());
    EXPECT_EQ(0x90, UMP_MIDI_STATUS(_transport.tx_packets.at(0)));
    EXPECT_EQ(60, UMP_MIDI1_P1(_transport.tx_packets.at(0)));
    EXPECT_EQ(100, UMP_MIDI1_P2(_transport.tx_packets.at(0)));
    EXPECT_EQ(0x90, UMP_MIDI_STATUS(_transport.tx_packets.at(1)));
    EXPECT_EQ(61, UMP_MIDI1_P1(_transport.tx_packets.at(1)));
    EXPECT_EQ(101, UMP_MIDI1_P2(_transport.tx_packets.at(1)));
}

TEST_F(MidiBaseTest, NoteOffCanBeSentAsNoteOnWithZeroVelocity)
{
    ASSERT_TRUE(_midi.send(midi1::note_off(TEST_GROUP, 0, 60, 99, true)));

    ASSERT_EQ(1, _transport.tx_packets.size());
    EXPECT_EQ(0x90, UMP_MIDI_STATUS(_transport.tx_packets.at(0)));
    EXPECT_EQ(60, UMP_MIDI1_P1(_transport.tx_packets.at(0)));
    EXPECT_EQ(0, UMP_MIDI1_P2(_transport.tx_packets.at(0)));
}

TEST_F(MidiBaseTest, Send14BitControlChangeWritesCoarseAndFineControllers)
{
    const auto packets = midi1::control_change_14bit(TEST_GROUP, 1, 10, 0x1234);

    ASSERT_TRUE(_midi.send(packets.at(0)));
    ASSERT_TRUE(_midi.send(packets.at(1)));

    ASSERT_EQ(2, _transport.tx_packets.size());
    EXPECT_EQ(0xB1, UMP_MIDI_STATUS(_transport.tx_packets.at(0)));
    EXPECT_EQ(10, UMP_MIDI1_P1(_transport.tx_packets.at(0)));
    EXPECT_EQ(0x24, UMP_MIDI1_P2(_transport.tx_packets.at(0)));
    EXPECT_EQ(0xB1, UMP_MIDI_STATUS(_transport.tx_packets.at(1)));
    EXPECT_EQ(42, UMP_MIDI1_P1(_transport.tx_packets.at(1)));
    EXPECT_EQ(0x34, UMP_MIDI1_P2(_transport.tx_packets.at(1)));
}

TEST_F(MidiBaseTest, ReadStoresChannelUmp)
{
    _transport.rx_packets.push_back(midi1_channel_voice_ump(0, 0x91, 0x40, 0x7F));

    const auto packet = _midi.read();
    ASSERT_TRUE(packet.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(packet.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(packet.value()));
    EXPECT_EQ(1, UMP_MIDI_CHANNEL(packet.value()));
    EXPECT_EQ(0x40, UMP_MIDI1_P1(packet.value()));
    EXPECT_EQ(0x7F, UMP_MIDI1_P2(packet.value()));
}

TEST_F(MidiBaseTest, ReadStoresConsecutiveChannelUmps)
{
    _transport.rx_packets.push_back(midi1_channel_voice_ump(0, 0x92, 0x30, 0x40));
    _transport.rx_packets.push_back(midi1_channel_voice_ump(0, 0x92, 0x31, 0x41));

    const auto packet_1 = _midi.read();
    ASSERT_TRUE(packet_1.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(packet_1.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(packet_1.value()));
    EXPECT_EQ(2, UMP_MIDI_CHANNEL(packet_1.value()));
    EXPECT_EQ(0x30, UMP_MIDI1_P1(packet_1.value()));
    EXPECT_EQ(0x40, UMP_MIDI1_P2(packet_1.value()));

    const auto packet_2 = _midi.read();
    ASSERT_TRUE(packet_2.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(packet_2.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(packet_2.value()));
    EXPECT_EQ(2, UMP_MIDI_CHANNEL(packet_2.value()));
    EXPECT_EQ(0x31, UMP_MIDI1_P1(packet_2.value()));
    EXPECT_EQ(0x41, UMP_MIDI1_P2(packet_2.value()));
}

TEST_F(MidiBaseTest, ReadForwardsUmpToRegisteredThruInterface)
{
    FakeThru thru;
    _midi.register_thru_interface(thru);

    _transport.rx_packets.push_back(midi1_channel_voice_ump(0, 0x90, 0x3C, 0x64));

    const auto packet = _midi.read();
    ASSERT_TRUE(packet.has_value());

    ASSERT_EQ(1, thru.tx_packets.size());
    EXPECT_EQ(packet.value().data[0], thru.tx_packets.at(0).data[0]);
}

TEST_F(MidiBaseTest, UnregisterThruStopsForwarding)
{
    FakeThru thru;
    _midi.register_thru_interface(thru);
    _midi.unregister_thru_interface(thru);

    _transport.rx_packets.push_back(midi1_channel_voice_ump(0, 0x90, 0x3C, 0x64));

    ASSERT_TRUE(_midi.read().has_value());
    EXPECT_TRUE(thru.tx_packets.empty());
}

TEST_F(MidiBaseTest, ConcurrentSendDoesNotInterleaveUmpPackets)
{
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
                                     send_worker(context_1);
                                 });

        MidiSendThread2 thread_2([&]()
                                 {
                                     send_worker(context_2);
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

    const size_t expected_messages = SEND_THREAD_COUNT * SEND_ITERATIONS_PER_THREAD;

    ASSERT_EQ(expected_messages, _transport.tx_packets.size());

    size_t thread_1_messages = 0;
    size_t thread_2_messages = 0;

    for (const auto& packet : _transport.tx_packets)
    {
        const uint8_t status = UMP_MIDI_STATUS(packet);
        const uint8_t data1  = UMP_MIDI1_P1(packet);
        const uint8_t data2  = UMP_MIDI1_P2(packet);

        if (status == 0x90)
        {
            EXPECT_EQ(THREAD_1_NOTE, data1);
            EXPECT_EQ(THREAD_1_VELOCITY, data2);
            thread_1_messages++;
        }
        else if (status == 0x91)
        {
            EXPECT_EQ(THREAD_2_NOTE, data1);
            EXPECT_EQ(THREAD_2_VELOCITY, data2);
            thread_2_messages++;
        }
        else
        {
            ADD_FAILURE() << "Unexpected status byte: " << static_cast<int>(status);
        }
    }

    EXPECT_EQ(SEND_ITERATIONS_PER_THREAD, thread_1_messages);
    EXPECT_EQ(SEND_ITERATIONS_PER_THREAD, thread_2_messages);
}
