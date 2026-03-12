/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "midi_common.h"

#include <array>

namespace zlibs::utils::midi::midi1
{
    /**
     * @brief Compile-time constraint for MIDI 1 channel voice message types.
     */
    template<MessageType Type>
    concept ChannelVoiceMessageType = (Type == MessageType::NoteOff) || (Type == MessageType::NoteOn) ||
                                      (Type == MessageType::ControlChange) || (Type == MessageType::ProgramChange) ||
                                      (Type == MessageType::AfterTouchChannel) || (Type == MessageType::AfterTouchPoly) ||
                                      (Type == MessageType::PitchBend);

    /**
     * @brief Compile-time constraint for MIDI 1 system-common and system real-time message types.
     */
    template<MessageType Type>
    concept SystemMessageType = (Type == MessageType::SysCommonTimeCodeQuarterFrame) ||
                                (Type == MessageType::SysCommonSongPosition) || (Type == MessageType::SysCommonSongSelect) ||
                                (Type == MessageType::SysCommonTuneRequest) || (Type == MessageType::SysRealTimeClock) ||
                                (Type == MessageType::SysRealTimeStart) || (Type == MessageType::SysRealTimeContinue) ||
                                (Type == MessageType::SysRealTimeStop) || (Type == MessageType::SysRealTimeActiveSensing) ||
                                (Type == MessageType::SysRealTimeSystemReset);

    /**
     * @brief Compile-time constraint for supported MIDI Machine Control commands.
     */
    template<MessageType Type>
    concept MmcMessageType = (Type == MessageType::MmcPlay) || (Type == MessageType::MmcStop) ||
                             (Type == MessageType::MmcPause) || (Type == MessageType::MmcRecordStart) ||
                             (Type == MessageType::MmcRecordStop);

    /**
     * @brief Builds one MIDI 1 channel voice UMP.
     *
     * @tparam Type MIDI 1 channel voice message type.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param data1 First MIDI data byte.
     * @param data2 Second MIDI data byte.
     *
     * @return Encoded MIDI 1 channel voice UMP.
     */
    template<MessageType Type>
        requires ChannelVoiceMessageType<Type>
    constexpr midi_ump channel_voice(uint8_t group, uint8_t channel, uint8_t data1, uint8_t data2)
    {
        const uint8_t status = static_cast<uint8_t>(Type) | (channel & MIDI_LOW_NIBBLE_MASK);
        return midi1_channel_voice_ump(group, status, data1, data2);
    }

    /**
     * @brief Builds one MIDI 1 Note On UMP.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param note_number MIDI note number.
     * @param velocity Note velocity.
     *
     * @return Encoded Note On UMP.
     */
    constexpr midi_ump note_on(uint8_t group, uint8_t channel, uint8_t note_number, uint8_t velocity)
    {
        return channel_voice<MessageType::NoteOn>(group, channel, note_number, velocity);
    }

    /**
     * @brief Builds one MIDI 1 Note Off UMP.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param note_number MIDI note number.
     * @param velocity Note release velocity.
     * @param send_as_note_on When `true`, encodes note release as Note On with zero velocity.
     *
     * @return Encoded Note Off UMP.
     */
    constexpr midi_ump note_off(uint8_t group, uint8_t channel, uint8_t note_number, uint8_t velocity, bool send_as_note_on = false)
    {
        if (send_as_note_on)
        {
            return note_on(group, channel, note_number, 0);
        }

        return channel_voice<MessageType::NoteOff>(group, channel, note_number, velocity);
    }

    /**
     * @brief Builds one MIDI 1 Program Change UMP.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param program_number MIDI program number.
     *
     * @return Encoded Program Change UMP.
     */
    constexpr midi_ump program_change(uint8_t group, uint8_t channel, uint8_t program_number)
    {
        return channel_voice<MessageType::ProgramChange>(group, channel, program_number, 0);
    }

    /**
     * @brief Builds one MIDI 1 Control Change UMP.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param control_number Controller number.
     * @param control_value Controller value.
     *
     * @return Encoded Control Change UMP.
     */
    constexpr midi_ump control_change(uint8_t group, uint8_t channel, uint8_t control_number, uint8_t control_value)
    {
        return channel_voice<MessageType::ControlChange>(group, channel, control_number, control_value);
    }

    /**
     * @brief Builds one 14-bit MIDI 1 Control Change sequence.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param control_number Coarse controller number.
     * @param control_value 14-bit controller value.
     *
     * @return Encoded coarse/fine Control Change UMP sequence.
     */
    constexpr std::array<midi_ump, 2> control_change_14bit(uint8_t group, uint8_t channel, uint16_t control_number, uint16_t control_value)
    {
        const auto split = Split14Bit(control_value);

        return {
            control_change(group, channel, control_number, split.high()),
            control_change(group, channel, control_number + CONTROL_CHANGE_LSB_OFFSET, split.low()),
        };
    }

    /**
     * @brief Builds one MIDI 1 Polyphonic AfterTouch UMP.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param note_number Target MIDI note number.
     * @param pressure AfterTouch pressure.
     *
     * @return Encoded Polyphonic AfterTouch UMP.
     */
    constexpr midi_ump poly_after_touch(uint8_t group, uint8_t channel, uint8_t note_number, uint8_t pressure)
    {
        return channel_voice<MessageType::AfterTouchPoly>(group, channel, note_number, pressure);
    }

    /**
     * @brief Builds one MIDI 1 Channel AfterTouch UMP.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param pressure Channel pressure value.
     *
     * @return Encoded Channel AfterTouch UMP.
     */
    constexpr midi_ump channel_after_touch(uint8_t group, uint8_t channel, uint8_t pressure)
    {
        return channel_voice<MessageType::AfterTouchChannel>(group, channel, pressure, 0);
    }

    /**
     * @brief Builds one MIDI 1 Pitch Bend UMP.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param pitch_value 14-bit pitch bend value.
     *
     * @return Encoded Pitch Bend UMP.
     */
    constexpr midi_ump pitch_bend(uint8_t group, uint8_t channel, uint16_t pitch_value)
    {
        const auto split = Split14Bit(pitch_value & MAX_VALUE_14BIT);
        return channel_voice<MessageType::PitchBend>(group, channel, split.low(), split.high());
    }

    /**
     * @brief Builds one MIDI 1 System Common/Real-Time UMP.
     *
     * @tparam Type MIDI 1 system-common or system real-time message type.
     *
     * @param group UMP group number.
     * @param data1 First MIDI data byte.
     * @param data2 Second MIDI data byte.
     *
     * @return Encoded system UMP.
     */
    template<MessageType Type>
        requires SystemMessageType<Type>
    constexpr midi_ump system(uint8_t group, uint8_t data1 = 0, uint8_t data2 = 0)
    {
        return system_ump(group, static_cast<uint8_t>(Type), data1, data2);
    }

    /**
     * @brief Builds one MIDI 1 tune request UMP.
     *
     * @param group UMP group number.
     *
     * @return Encoded Tune Request UMP.
     */
    constexpr midi_ump tune_request(uint8_t group)
    {
        return system<MessageType::SysCommonTuneRequest>(group);
    }

    /**
     * @brief Builds one MIDI 1 time-code quarter-frame UMP from nibbles.
     *
     * @param group UMP group number.
     * @param type_nibble Quarter-frame type nibble.
     * @param values_nibble Quarter-frame value nibble.
     *
     * @return Encoded time-code quarter-frame UMP.
     */
    constexpr midi_ump time_code_quarter_frame(uint8_t group, uint8_t type_nibble, uint8_t values_nibble)
    {
        const uint8_t data = ((type_nibble & MTC_TYPE_MASK) << MIDI_STATUS_TYPE_SHIFT) | (values_nibble & MTC_DATA_MASK);
        return system<MessageType::SysCommonTimeCodeQuarterFrame>(group, data);
    }

    /**
     * @brief Builds one MIDI 1 time-code quarter-frame UMP from raw data byte.
     *
     * @param group UMP group number.
     * @param data Raw quarter-frame byte.
     *
     * @return Encoded time-code quarter-frame UMP.
     */
    constexpr midi_ump time_code_quarter_frame(uint8_t group, uint8_t data)
    {
        return system<MessageType::SysCommonTimeCodeQuarterFrame>(group, data);
    }

    /**
     * @brief Builds one MIDI 1 Song Position Pointer UMP.
     *
     * @param group UMP group number.
     * @param beats 14-bit song position value.
     *
     * @return Encoded Song Position Pointer UMP.
     */
    constexpr midi_ump song_position(uint8_t group, uint16_t beats)
    {
        const uint8_t data_1 = beats & MAX_VALUE_7BIT;
        const uint8_t data_2 = (beats >> DATA_MSB_SHIFT) & MAX_VALUE_7BIT;
        return system<MessageType::SysCommonSongPosition>(group, data_1, data_2);
    }

    /**
     * @brief Builds one MIDI 1 Song Select UMP.
     *
     * @param group UMP group number.
     * @param song_number Song index.
     *
     * @return Encoded Song Select UMP.
     */
    constexpr midi_ump song_select(uint8_t group, uint8_t song_number)
    {
        return system<MessageType::SysCommonSongSelect>(group, song_number);
    }

    /**
     * @brief Builds one MIDI 1 Timing Clock UMP.
     *
     * @param group UMP group number.
     *
     * @return Encoded Timing Clock UMP.
     */
    constexpr midi_ump timing_clock(uint8_t group)
    {
        return system<MessageType::SysRealTimeClock>(group);
    }

    /**
     * @brief Builds one MIDI 1 Start UMP.
     *
     * @param group UMP group number.
     *
     * @return Encoded Start UMP.
     */
    constexpr midi_ump start(uint8_t group)
    {
        return system<MessageType::SysRealTimeStart>(group);
    }

    /**
     * @brief Builds one MIDI 1 Continue UMP.
     *
     * @param group UMP group number.
     *
     * @return Encoded Continue UMP.
     */
    constexpr midi_ump continue_playback(uint8_t group)
    {
        return system<MessageType::SysRealTimeContinue>(group);
    }

    /**
     * @brief Builds one MIDI 1 Stop UMP.
     *
     * @param group UMP group number.
     *
     * @return Encoded Stop UMP.
     */
    constexpr midi_ump stop(uint8_t group)
    {
        return system<MessageType::SysRealTimeStop>(group);
    }

    /**
     * @brief Builds one MIDI 1 Active Sensing UMP.
     *
     * @param group UMP group number.
     *
     * @return Encoded Active Sensing UMP.
     */
    constexpr midi_ump active_sensing(uint8_t group)
    {
        return system<MessageType::SysRealTimeActiveSensing>(group);
    }

    /**
     * @brief Builds one MIDI 1 System Reset UMP.
     *
     * @param group UMP group number.
     *
     * @return Encoded System Reset UMP.
     */
    constexpr midi_ump system_reset(uint8_t group)
    {
        return system<MessageType::SysRealTimeSystemReset>(group);
    }

    /**
     * @brief Builds one complete SysEx7 Data64 UMP.
     *
     * @param group UMP group number.
     * @param payload SysEx7 payload bytes, excluding SysEx start/end bytes.
     *
     * @return Encoded complete SysEx7 Data64 UMP, or `std::nullopt` when payload is too large.
     */
    inline std::optional<midi_ump> sysex7_complete(uint8_t group, std::span<const uint8_t> payload)
    {
        return sysex7_complete_ump(group, payload);
    }

    /**
     * @brief Builds one MIDI Machine Control SysEx7/Data64 UMP.
     *
     * @param group UMP group number.
     * @param device_id MMC target device identifier.
     * @tparam Type MMC command type.
     *
     * @param device_id MMC target device identifier.
     *
     * @return Encoded MMC SysEx7/Data64 UMP.
     */
    template<MessageType Type>
        requires MmcMessageType<Type>
    inline std::optional<midi_ump> mmc(uint8_t group, uint8_t device_id)
    {
        const std::array<uint8_t, 4> payload = {
            MMC_ALL_CALL_DEVICE_ID,
            device_id,
            MMC_SUB_ID,
            static_cast<uint8_t>(Type),
        };

        return sysex7_complete(group, payload);
    }

    /**
     * @brief Builds one 7-bit NRPN packet sequence.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param parameter_number 14-bit NRPN parameter number.
     * @param value 7-bit NRPN value.
     *
     * @return Encoded 7-bit NRPN UMP sequence.
     */
    constexpr std::array<midi_ump, 3> nrpn_7bit(uint8_t group, uint8_t channel, uint16_t parameter_number, uint8_t value)
    {
        const auto parameter = Split14Bit(parameter_number);

        return {
            control_change(group, channel, NRPN_MSB_CONTROLLER, parameter.high()),
            control_change(group, channel, NRPN_LSB_CONTROLLER, parameter.low()),
            control_change(group, channel, DATA_ENTRY_MSB_CONTROLLER, value),
        };
    }

    /**
     * @brief Builds one 14-bit NRPN packet sequence.
     *
     * @param group UMP group number.
     * @param channel Zero-based MIDI channel number.
     * @param parameter_number 14-bit NRPN parameter number.
     * @param value 14-bit NRPN value.
     *
     * @return Encoded 14-bit NRPN UMP sequence.
     */
    constexpr std::array<midi_ump, 4> nrpn_14bit(uint8_t group, uint8_t channel, uint16_t parameter_number, uint16_t value)
    {
        const auto parameter   = Split14Bit(parameter_number);
        const auto split_value = Split14Bit(value);

        return {
            control_change(group, channel, NRPN_MSB_CONTROLLER, parameter.high()),
            control_change(group, channel, NRPN_LSB_CONTROLLER, parameter.low()),
            control_change(group, channel, DATA_ENTRY_MSB_CONTROLLER, split_value.high()),
            control_change(group, channel, DATA_ENTRY_LSB_CONTROLLER, split_value.low()),
        };
    }
}    // namespace zlibs::utils::midi::midi1
