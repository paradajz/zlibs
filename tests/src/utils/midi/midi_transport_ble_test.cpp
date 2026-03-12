/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/midi/transport/ble/transport_ble.h"

#include <vector>

using namespace ::testing;
using namespace zlibs::utils::midi;

namespace
{
    LOG_MODULE_REGISTER(midi_ble_test, CONFIG_ZLIBS_LOG_LEVEL);

    constexpr uint8_t TEST_GROUP = 0;

    using BlePacket = zlibs::utils::midi::ble::Packet;

    class MidiBleTransportTest : public Test
    {
        protected:
        class BleHwa : public zlibs::utils::midi::ble::Hwa
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

            bool write(BlePacket& packet) override
            {
                write_packets.push_back(packet);
                return true;
            }

            std::optional<BlePacket> read() override
            {
                if (read_packets.empty())
                {
                    return {};
                }

                const auto packet = read_packets.front();
                read_packets.erase(read_packets.begin());

                return packet;
            }

            std::vector<BlePacket> write_packets = {};
            std::vector<BlePacket> read_packets  = {};
        };

        void SetUp() override
        {
            ASSERT_TRUE(_midi.init());
        }

        BleHwa                       _hwa;
        zlibs::utils::midi::ble::Ble _midi = zlibs::utils::midi::ble::Ble(_hwa);
    };
}    // namespace

TEST_F(MidiBleTransportTest, ReadSingleUmp)
{
    BlePacket packet = {};

    packet.data.at(packet.size++) = 0x80;
    packet.data.at(packet.size++) = 0x80;
    packet.data.at(packet.size++) = 0x90;
    packet.data.at(packet.size++) = 0x00;
    packet.data.at(packet.size++) = 0x7F;

    _hwa.read_packets.push_back(packet);

    const auto read_packet = _midi.read();
    ASSERT_TRUE(read_packet.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(read_packet.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(read_packet.value()));
    EXPECT_EQ(0, UMP_MIDI_CHANNEL(read_packet.value()));
    EXPECT_EQ(0, UMP_MIDI1_P1(read_packet.value()));
    EXPECT_EQ(0x7F, UMP_MIDI1_P2(read_packet.value()));

    ASSERT_FALSE(_midi.read().has_value());
}

TEST_F(MidiBleTransportTest, ReadTwoUmpsWithRunningStatus)
{
    BlePacket packet = {};

    packet.data.at(packet.size++) = 0x80;
    packet.data.at(packet.size++) = 0x80;
    packet.data.at(packet.size++) = 0x90;
    packet.data.at(packet.size++) = 0x00;
    packet.data.at(packet.size++) = 0x7F;
    packet.data.at(packet.size++) = 0x80;
    packet.data.at(packet.size++) = 0x00;
    packet.data.at(packet.size++) = 0x7E;

    _hwa.read_packets.push_back(packet);

    const auto packet_1 = _midi.read();
    ASSERT_TRUE(packet_1.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(packet_1.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(packet_1.value()));
    EXPECT_EQ(0, UMP_MIDI_CHANNEL(packet_1.value()));
    EXPECT_EQ(0, UMP_MIDI1_P1(packet_1.value()));
    EXPECT_EQ(0x7F, UMP_MIDI1_P2(packet_1.value()));

    const auto packet_2 = _midi.read();
    ASSERT_TRUE(packet_2.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(packet_2.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(packet_2.value()));
    EXPECT_EQ(0, UMP_MIDI_CHANNEL(packet_2.value()));
    EXPECT_EQ(0, UMP_MIDI1_P1(packet_2.value()));
    EXPECT_EQ(0x7E, UMP_MIDI1_P2(packet_2.value()));
}

TEST_F(MidiBleTransportTest, ReadSysExUmp)
{
    constexpr std::array<uint8_t, 3> PAYLOAD = { 0x01, 0x02, 0x03 };

    BlePacket packet = {};

    packet.data.at(packet.size++) = 0x80;
    packet.data.at(packet.size++) = 0x80;
    packet.data.at(packet.size++) = SYS_EX_START;
    packet.data.at(packet.size++) = PAYLOAD.at(0);
    packet.data.at(packet.size++) = PAYLOAD.at(1);
    packet.data.at(packet.size++) = PAYLOAD.at(2);
    packet.data.at(packet.size++) = SYS_EX_END;

    _hwa.read_packets.push_back(packet);

    const auto packet_out = _midi.read();
    ASSERT_TRUE(packet_out.has_value());

    const auto expected_packet = sysex7_complete_ump(0, PAYLOAD);
    ASSERT_TRUE(expected_packet.has_value());
    EXPECT_EQ(UMP_MT_DATA_64, UMP_MT(packet_out.value()));
    EXPECT_EQ(expected_packet.value().data[0], packet_out.value().data[0]);
    EXPECT_EQ(expected_packet.value().data[1], packet_out.value().data[1]);
}

TEST_F(MidiBleTransportTest, SendNoteOnWritesBlePacket)
{
    ASSERT_TRUE(_midi.send(midi1::note_on(TEST_GROUP, 0, 60, 100)));

    ASSERT_EQ(1, _hwa.write_packets.size());

    const auto& packet = _hwa.write_packets.at(0);

    ASSERT_EQ(5, packet.size);
    EXPECT_TRUE(packet.data.at(0) & MIDI_STATUS_MIN);
    EXPECT_TRUE(packet.data.at(1) & MIDI_STATUS_MIN);
    EXPECT_EQ(0x90, packet.data.at(2));
    EXPECT_EQ(60, packet.data.at(3));
    EXPECT_EQ(100, packet.data.at(4));
}

TEST_F(MidiBleTransportTest, WriteUmpWritesBlePacket)
{
    ASSERT_TRUE(_midi.send(midi1::note_on(TEST_GROUP, 0, 60, 100)));

    ASSERT_EQ(1, _hwa.write_packets.size());

    const auto& packet = _hwa.write_packets.at(0);

    ASSERT_EQ(5, packet.size);
    EXPECT_TRUE(packet.data.at(0) & MIDI_STATUS_MIN);
    EXPECT_TRUE(packet.data.at(1) & MIDI_STATUS_MIN);
    EXPECT_EQ(0x90, packet.data.at(2));
    EXPECT_EQ(60, packet.data.at(3));
    EXPECT_EQ(100, packet.data.at(4));
}
