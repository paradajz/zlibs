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

    constexpr uint8_t TEST_GROUP = 0;

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

            bool write(const midi_ump& packet) override
            {
                write_packets.push_back(packet);
                return true;
            }

            std::optional<midi_ump> read() override
            {
                if (read_packets.empty())
                {
                    return {};
                }

                const auto PACKET = read_packets.front();
                read_packets.erase(read_packets.begin());

                return PACKET;
            }

            std::vector<midi_ump> write_packets = {};
            std::vector<midi_ump> read_packets  = {};
        };

        void SetUp() override
        {
            ASSERT_TRUE(_midi.init());
        }

        UsbHwa                       _hwa;
        zlibs::utils::midi::usb::Usb _midi = zlibs::utils::midi::usb::Usb(_hwa);
    };
}    // namespace

TEST_F(MidiUsbTransportTest, ReadSingleUmp)
{
    _hwa.read_packets.push_back(midi1_channel_voice_ump(0, 0x90, 0x3C, 0x64));

    const auto PACKET = _midi.read();
    ASSERT_TRUE(PACKET.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(PACKET.value()));
    EXPECT_EQ(UMP_MIDI_NOTE_ON, UMP_MIDI_COMMAND(PACKET.value()));
    EXPECT_EQ(0, UMP_MIDI_CHANNEL(PACKET.value()));
    EXPECT_EQ(0x3C, UMP_MIDI1_P1(PACKET.value()));
    EXPECT_EQ(0x64, UMP_MIDI1_P2(PACKET.value()));
}

TEST_F(MidiUsbTransportTest, ReadProgramChangeUmp)
{
    _hwa.read_packets.push_back(midi1_channel_voice_ump(0, 0xC0, 0x07, 0x00));

    const auto PACKET = _midi.read();
    ASSERT_TRUE(PACKET.has_value());
    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(PACKET.value()));
    EXPECT_EQ(UMP_MIDI_PROGRAM_CHANGE, UMP_MIDI_COMMAND(PACKET.value()));
    EXPECT_EQ(0, UMP_MIDI_CHANNEL(PACKET.value()));
    EXPECT_EQ(0x07, UMP_MIDI1_P1(PACKET.value()));
    EXPECT_EQ(0x00, UMP_MIDI1_P2(PACKET.value()));
}

TEST_F(MidiUsbTransportTest, SendNoteOnWritesUsbPacket)
{
    ASSERT_TRUE(_midi.send(midi1::note_on(TEST_GROUP, 0, 60, 127)));

    ASSERT_EQ(1, _hwa.write_packets.size());

    const auto& PACKET = _hwa.write_packets.at(0);

    EXPECT_EQ(UMP_MT_MIDI1_CHANNEL_VOICE, UMP_MT(PACKET));
    EXPECT_EQ(0, UMP_GROUP(PACKET));
    EXPECT_EQ(0x90, UMP_MIDI_STATUS(PACKET));
    EXPECT_EQ(60, UMP_MIDI1_P1(PACKET));
    EXPECT_EQ(127, UMP_MIDI1_P2(PACKET));
}

TEST_F(MidiUsbTransportTest, WriteUmpPassesPacketToHwa)
{
    const auto PACKET = midi1_channel_voice_ump(TEST_GROUP, 0x90, 60, 100);

    ASSERT_TRUE(_midi.send(PACKET));

    ASSERT_EQ(1, _hwa.write_packets.size());
    EXPECT_EQ(PACKET.data[0], _hwa.write_packets.at(0).data[0]);
}

TEST_F(MidiUsbTransportTest, ReadSysExCompleteUmp)
{
    constexpr std::array<uint8_t, 3> PAYLOAD = { 0x01, 0x02, 0x03 };
    const auto                       PACKET  = sysex7_complete_ump(0, PAYLOAD);

    ASSERT_TRUE(PACKET.has_value());
    _hwa.read_packets.push_back(PACKET.value());

    const auto READ_PACKET = _midi.read();
    ASSERT_TRUE(READ_PACKET.has_value());
    EXPECT_EQ(UMP_MT_DATA_64, UMP_MT(READ_PACKET.value()));
    EXPECT_EQ(PACKET.value().data[0], READ_PACKET.value().data[0]);
    EXPECT_EQ(PACKET.value().data[1], READ_PACKET.value().data[1]);
}

TEST_F(MidiUsbTransportTest, SendSysExWritesData64Ump)
{
    constexpr std::array<uint8_t, 3> PAYLOAD = { 0x01, 0x02, 0x03 };
    const auto                       PACKET  = midi1::sysex7_complete(TEST_GROUP, PAYLOAD);

    ASSERT_TRUE(PACKET.has_value());
    ASSERT_TRUE(_midi.send(PACKET.value()));
    ASSERT_EQ(1, _hwa.write_packets.size());

    const auto& WRITTEN_PACKET = _hwa.write_packets.at(0);

    EXPECT_EQ(UMP_MT_DATA_64, UMP_MT(WRITTEN_PACKET));
    EXPECT_EQ(0, UMP_GROUP(WRITTEN_PACKET));
    EXPECT_EQ(SysEx7Status::Complete, sysex7_status(WRITTEN_PACKET));
    EXPECT_EQ(3, sysex7_payload_size(WRITTEN_PACKET));
    EXPECT_EQ(0x01, read_sysex7_payload_byte(WRITTEN_PACKET, 0));
    EXPECT_EQ(0x02, read_sysex7_payload_byte(WRITTEN_PACKET, 1));
    EXPECT_EQ(0x03, read_sysex7_payload_byte(WRITTEN_PACKET, 2));
}
