/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/midi/transport/usb/transport_usb.h"

#include <vector>

using namespace ::testing;
using namespace zlibs::utils::midi;

namespace
{
    LOG_MODULE_REGISTER(midi_usb_test, CONFIG_ZLIBS_LOG_LEVEL);

    using UsbPacket = zlibs::utils::midi::usb::Packet;

    class MidiUsbTransportTest : public Test
    {
        protected:
        class UsbHwa : public zlibs::utils::midi::usb::Hwa
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

            bool write(UsbPacket& packet) override
            {
                write_packets.push_back(packet);
                return true;
            }

            std::optional<UsbPacket> read() override
            {
                if (read_packets.empty())
                {
                    return {};
                }

                const auto PACKET = read_packets.front();
                read_packets.erase(read_packets.begin());

                return PACKET;
            }

            std::vector<UsbPacket> write_packets = {};
            std::vector<UsbPacket> read_packets  = {};
        };

        void SetUp() override
        {
            ASSERT_TRUE(_midi.init());
        }

        UsbHwa                       _hwa;
        zlibs::utils::midi::usb::Usb _midi = zlibs::utils::midi::usb::Usb(_hwa);
    };
}    // namespace

TEST_F(MidiUsbTransportTest, ReadSingleMessage)
{
    _hwa.read_packets.push_back({ { 0x09, 0x90, 0x3C, 0x64 } });

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(1, _midi.message().channel);
    EXPECT_EQ(MessageType::NoteOn, _midi.message().type);
    EXPECT_EQ(0x3C, _midi.message().data1);
    EXPECT_EQ(0x64, _midi.message().data2);
}

TEST_F(MidiUsbTransportTest, ReadProgramChangeMessage)
{
    _hwa.read_packets.push_back({ { 0x0C, 0xC0, 0x07, 0x00 } });

    ASSERT_TRUE(_midi.read());
    EXPECT_EQ(1, _midi.message().channel);
    EXPECT_EQ(MessageType::ProgramChange, _midi.message().type);
    EXPECT_EQ(0x07, _midi.message().data1);
    EXPECT_EQ(0x00, _midi.message().data2);
}

TEST_F(MidiUsbTransportTest, SendNoteOnWritesUsbPacket)
{
    ASSERT_TRUE(_midi.send_note_on(60, 127, 1));

    ASSERT_EQ(1, _hwa.write_packets.size());

    const auto& PACKET = _hwa.write_packets.at(0);

    EXPECT_EQ(0x09, PACKET.data[UsbPacket::Event]);
    EXPECT_EQ(0x90, PACKET.data[UsbPacket::Data1]);
    EXPECT_EQ(60, PACKET.data[UsbPacket::Data2]);
    EXPECT_EQ(127, PACKET.data[UsbPacket::Data3]);
}
