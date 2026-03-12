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

                const auto PACKET = read_packets.front();
                read_packets.erase(read_packets.begin());

                return PACKET;
            }

            uint32_t time() override
            {
                return 0x80;
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

TEST_F(MidiBleTransportTest, ReadSingleMessage)
{
    BlePacket packet = {};

    packet.data.at(packet.size++) = 0x80;
    packet.data.at(packet.size++) = 0x80;
    packet.data.at(packet.size++) = 0x90;
    packet.data.at(packet.size++) = 0x00;
    packet.data.at(packet.size++) = 0x7F;

    _hwa.read_packets.push_back(packet);

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(1, _midi.message().channel);
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(0, _midi.message().data1);
    EXPECT_EQ(0x7F, _midi.message().data2);

    ASSERT_FALSE(_midi.read());
}

TEST_F(MidiBleTransportTest, ReadTwoMessagesWithRunningStatus)
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

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(1, _midi.message().channel);
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(0, _midi.message().data1);
    EXPECT_EQ(0x7F, _midi.message().data2);

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(1, _midi.message().channel);
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(0, _midi.message().data1);
    EXPECT_EQ(0x7E, _midi.message().data2);
}

TEST_F(MidiBleTransportTest, SendNoteOnWritesSingleBlePacket)
{
    ASSERT_TRUE(_midi.send_note_on(60, 100, 1));

    ASSERT_EQ(1, _hwa.write_packets.size());

    const auto& PACKET = _hwa.write_packets.at(0);

    ASSERT_EQ(5, PACKET.size);
    EXPECT_EQ(0x90, PACKET.data[2]);
    EXPECT_EQ(60, PACKET.data[3]);
    EXPECT_EQ(100, PACKET.data[4]);
}
