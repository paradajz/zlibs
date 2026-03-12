/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/midi/midi_common.h"

using namespace ::testing;
using namespace zlibs::utils::midi;

TEST(MidiCommonTest, SplitAndMerge14BitRoundTrip)
{
    const std::array<uint16_t, 6> TEST_VALUES = { 0, 1, 127, 128, 8192, MAX_VALUE_14BIT };

    for (const auto VALUE : TEST_VALUES)
    {
        Split14Bit split(VALUE);
        Merge14Bit merged(split.high(), split.low());

        EXPECT_EQ(VALUE, merged.value());
    }
}

TEST(MidiCommonTest, TypeFromStatusByteParsesChannelAndSystemTypes)
{
    EXPECT_EQ(MessageType::NoteOn, type_from_status_byte(0x91));
    EXPECT_EQ(MessageType::ControlChange, type_from_status_byte(0xBF));
    EXPECT_EQ(MessageType::SysEx, type_from_status_byte(0xF0));
    EXPECT_EQ(MessageType::SysRealTimeStart, type_from_status_byte(0xFA));
}

TEST(MidiCommonTest, TypeFromStatusByteRejectsInvalidStatuses)
{
    EXPECT_EQ(MessageType::Invalid, type_from_status_byte(0x00));
    EXPECT_EQ(MessageType::Invalid, type_from_status_byte(0x7F));
    EXPECT_EQ(MessageType::Invalid, type_from_status_byte(0xF4));
    EXPECT_EQ(MessageType::Invalid, type_from_status_byte(0xF5));
    EXPECT_EQ(MessageType::Invalid, type_from_status_byte(0xF9));
    EXPECT_EQ(MessageType::Invalid, type_from_status_byte(0xFD));
}

TEST(MidiCommonTest, TypeParsesSupportedUmpPackets)
{
    EXPECT_EQ(MessageType::NoteOn, type(midi1_channel_voice_ump(0, 0x91, 0x3C, 0x64)));
    EXPECT_EQ(MessageType::SysCommonSongSelect, type(system_ump(0, static_cast<uint8_t>(MessageType::SysCommonSongSelect), 0x07, 0)));

    constexpr std::array<uint8_t, 3> PAYLOAD = { 0x01, 0x02, 0x03 };
    const auto                       PACKET  = sysex7_complete_ump(0, PAYLOAD);

    ASSERT_TRUE(PACKET.has_value());
    EXPECT_EQ(MessageType::SysEx, type(PACKET.value()));
}

TEST(MidiCommonTest, TypeRejectsUnsupportedUmpPackets)
{
    const midi_ump PACKET = {
        {
            static_cast<uint32_t>(UMP_MT_UTILITY) << UMP_MESSAGE_TYPE_SHIFT,
        },
    };

    EXPECT_EQ(MessageType::Invalid, type(PACKET));
}

TEST(MidiCommonTest, ChannelFromStatusByteIsOneBased)
{
    EXPECT_EQ(1, channel_from_status_byte(0x90));
    EXPECT_EQ(16, channel_from_status_byte(0x9F));
}

TEST(MidiCommonTest, NoteHelpersUseWrappedMidiRange)
{
    EXPECT_EQ(0, note_to_octave(0));
    EXPECT_EQ(10, note_to_octave(127));
    EXPECT_EQ(Note::C, note_to_tonic(0));
    EXPECT_EQ(Note::G, note_to_tonic(127));
}
