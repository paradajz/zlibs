/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/midi/transport/serial/transport_serial.h"

#include <vector>

using namespace ::testing;
using namespace zlibs::utils::midi;

namespace
{
    LOG_MODULE_REGISTER(midi_serial_test, CONFIG_ZLIBS_LOG_LEVEL);

    constexpr uint8_t TEST_GROUP = 0;

    class MidiSerialTransportTest : public Test
    {
        protected:
        class SerialHwa : public zlibs::utils::midi::serial::Hwa
        {
            public:
            bool init() override
            {
                return true;
            }

            bool deinit() override
            {
                return true;
            }

            bool write(uint8_t data) override
            {
                write_packets.push_back(data);
                return true;
            }

            std::optional<uint8_t> read() override
            {
                if (read_packets.empty())
                {
                    return {};
                }

                const auto DATA = read_packets.front();
                read_packets.erase(read_packets.begin());

                return DATA;
            }

            std::vector<uint8_t> write_packets = {};
            std::vector<uint8_t> read_packets  = {};
        };

        void SetUp() override
        {
            ASSERT_TRUE(_midi.init());
        }

        SerialHwa                          _hwa;
        zlibs::utils::midi::serial::Serial _midi = zlibs::utils::midi::serial::Serial(_hwa);
    };
}    // namespace

TEST_F(MidiSerialTransportTest, ReadSingleUmp)
{
    _hwa.read_packets.push_back(0x90);
    _hwa.read_packets.push_back(0x3C);
    _hwa.read_packets.push_back(0x64);

    const auto PACKET = _midi.read();
    ASSERT_TRUE(PACKET.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(PACKET.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(PACKET.value()));
    EXPECT_EQ(0, UMP_MIDI_CHANNEL(PACKET.value()));
    EXPECT_EQ(0x3C, UMP_MIDI1_P1(PACKET.value()));
    EXPECT_EQ(0x64, UMP_MIDI1_P2(PACKET.value()));
}

TEST_F(MidiSerialTransportTest, ReadTwoUmpsWithRunningStatus)
{
    _hwa.read_packets.push_back(0x90);
    _hwa.read_packets.push_back(0x30);
    _hwa.read_packets.push_back(0x40);
    _hwa.read_packets.push_back(0x31);
    _hwa.read_packets.push_back(0x41);

    const auto PACKET_1 = _midi.read();
    ASSERT_TRUE(PACKET_1.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(PACKET_1.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(PACKET_1.value()));
    EXPECT_EQ(0x30, UMP_MIDI1_P1(PACKET_1.value()));
    EXPECT_EQ(0x40, UMP_MIDI1_P2(PACKET_1.value()));

    const auto PACKET_2 = _midi.read();
    ASSERT_TRUE(PACKET_2.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(PACKET_2.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(PACKET_2.value()));
    EXPECT_EQ(0x31, UMP_MIDI1_P1(PACKET_2.value()));
    EXPECT_EQ(0x41, UMP_MIDI1_P2(PACKET_2.value()));
}

TEST_F(MidiSerialTransportTest, ReadSysExUmp)
{
    constexpr std::array<uint8_t, 3> PAYLOAD = { 0x01, 0x02, 0x03 };

    _hwa.read_packets.push_back(SYS_EX_START);
    _hwa.read_packets.push_back(PAYLOAD.at(0));
    _hwa.read_packets.push_back(PAYLOAD.at(1));
    _hwa.read_packets.push_back(PAYLOAD.at(2));
    _hwa.read_packets.push_back(SYS_EX_END);

    const auto PACKET = _midi.read();
    ASSERT_TRUE(PACKET.has_value());

    const auto EXPECTED_PACKET = sysex7_complete_ump(0, PAYLOAD);
    ASSERT_TRUE(EXPECTED_PACKET.has_value());
    EXPECT_EQ(UMP_MT_DATA_64, UMP_MT(PACKET.value()));
    EXPECT_EQ(EXPECTED_PACKET.value().data[0], PACKET.value().data[0]);
    EXPECT_EQ(EXPECTED_PACKET.value().data[1], PACKET.value().data[1]);
}

TEST_F(MidiSerialTransportTest, SendNoteOnWritesThreeSerialBytes)
{
    ASSERT_TRUE(_midi.send(midi1::note_on(TEST_GROUP, 1, 62, 90)));

    ASSERT_EQ(3, _hwa.write_packets.size());
    EXPECT_EQ(0x91, _hwa.write_packets.at(0));
    EXPECT_EQ(62, _hwa.write_packets.at(1));
    EXPECT_EQ(90, _hwa.write_packets.at(2));
}
