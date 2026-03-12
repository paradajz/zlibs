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
