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

            bool write(zlibs::utils::midi::serial::Packet& data) override
            {
                write_packets.push_back(data);
                return true;
            }

            std::optional<zlibs::utils::midi::serial::Packet> read() override
            {
                if (read_packets.empty())
                {
                    return {};
                }

                const auto DATA = read_packets.front();
                read_packets.erase(read_packets.begin());

                return DATA;
            }

            std::vector<zlibs::utils::midi::serial::Packet> write_packets = {};
            std::vector<zlibs::utils::midi::serial::Packet> read_packets  = {};
        };

        void SetUp() override
        {
            ASSERT_TRUE(_midi.init());
        }

        SerialHwa                          _hwa;
        zlibs::utils::midi::serial::Serial _midi = zlibs::utils::midi::serial::Serial(_hwa);
    };
}    // namespace

TEST_F(MidiSerialTransportTest, ReadSingleMessage)
{
    _hwa.read_packets.push_back({ 0x90 });
    _hwa.read_packets.push_back({ 0x3C });
    _hwa.read_packets.push_back({ 0x64 });

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(1, _midi.message().channel);
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(0x3C, _midi.message().data1);
    EXPECT_EQ(0x64, _midi.message().data2);
}

TEST_F(MidiSerialTransportTest, ReadTwoMessagesWithRunningStatus)
{
    _hwa.read_packets.push_back({ 0x90 });
    _hwa.read_packets.push_back({ 0x30 });
    _hwa.read_packets.push_back({ 0x40 });
    _hwa.read_packets.push_back({ 0x31 });
    _hwa.read_packets.push_back({ 0x41 });

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(0x30, _midi.message().data1);
    EXPECT_EQ(0x40, _midi.message().data2);

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(0x31, _midi.message().data1);
    EXPECT_EQ(0x41, _midi.message().data2);
}

TEST_F(MidiSerialTransportTest, SendNoteOnWritesThreeSerialPackets)
{
    ASSERT_TRUE(_midi.send_note_on(62, 90, 2));

    ASSERT_EQ(3, _hwa.write_packets.size());
    EXPECT_EQ(0x91, _hwa.write_packets.at(0).data);
    EXPECT_EQ(62, _hwa.write_packets.at(1).data);
    EXPECT_EQ(90, _hwa.write_packets.at(2).data);
}
