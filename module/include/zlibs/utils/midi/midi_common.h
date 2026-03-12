/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/audio/midi.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace zlibs::utils::midi
{
    /** @brief Number of bits in one byte. */
    constexpr inline uint8_t BITS_IN_BYTE = 8;

    /** @brief Mask for valid 7-bit MIDI data-byte payload. */
    constexpr inline uint8_t MIDI_DATA_BYTE_MASK = 0x7F;

    /** @brief Mask for extracting a low 4-bit nibble. */
    constexpr inline uint8_t MIDI_LOW_NIBBLE_MASK = 0x0F;

    /** @brief Mask for extracting the channel nibble from a MIDI status byte. */
    constexpr inline uint8_t MIDI_CHANNEL_NIBBLE_FROM_STATUS_MASK = MIDI_LOW_NIBBLE_MASK;

    /** @brief Mask for extracting message-type nibble from channel status byte. */
    constexpr inline uint8_t MIDI_STATUS_CHANNEL_MASK = 0xF0;

    /** @brief Minimum value for MIDI status bytes (`0x80`). */
    constexpr inline uint8_t MIDI_STATUS_MIN = 0x80;

    /** @brief Undefined system status byte (`0xF4`) reserved by MIDI spec. */
    constexpr inline uint8_t MIDI_STATUS_UNDEFINED_0 = 0xF4;

    /** @brief Undefined system status byte (`0xF5`) reserved by MIDI spec. */
    constexpr inline uint8_t MIDI_STATUS_UNDEFINED_1 = 0xF5;

    /** @brief Undefined system status byte (`0xF9`) reserved by MIDI spec. */
    constexpr inline uint8_t MIDI_STATUS_UNDEFINED_2 = 0xF9;

    /** @brief Undefined system status byte (`0xFD`) reserved by MIDI spec. */
    constexpr inline uint8_t MIDI_STATUS_UNDEFINED_3 = 0xFD;

    /** @brief First system-message status value (`0xF0`). */
    constexpr inline uint8_t MIDI_SYSTEM_MESSAGE_MIN = 0xF0;

    /** @brief Maximum value representable with 7 bits. */
    constexpr inline uint8_t MAX_VALUE_7BIT = 127;

    /** @brief Maximum value representable with 14 bits. */
    constexpr inline uint16_t MAX_VALUE_14BIT = 16383;

    /** @brief Default UMP group assigned when a MIDI 1 byte-stream transport has no group metadata. */
    constexpr uint8_t DEFAULT_RX_GROUP = 0;

    /** @brief System Exclusive start status byte (`0xF0`). */
    constexpr uint8_t SYS_EX_START = 0xF0;

    /** @brief System Exclusive end status byte (`0xF7`). */
    constexpr uint8_t SYS_EX_END = 0xF7;

    /** @brief Bit shift used to extract or place MSB in 14-bit split/merge operations. */
    constexpr uint8_t DATA_MSB_SHIFT = 7;

    /** @brief Mask for MTC quarter-frame type nibble. */
    constexpr uint8_t MTC_TYPE_MASK = 0x07;

    /** @brief Mask for MTC quarter-frame data nibble. */
    constexpr uint8_t MTC_DATA_MASK = 0x0F;

    /** @brief Fixed byte count for generated MMC SysEx frame. */
    constexpr uint8_t MMC_MESSAGE_LENGTH = 6;

    /** @brief MMC broadcast/all-call device identifier. */
    constexpr uint8_t MMC_ALL_CALL_DEVICE_ID = 0x7F;

    /** @brief MMC SysEx Sub-ID value. */
    constexpr uint8_t MMC_SUB_ID = 0x06;

    /** @brief Default MMC command placeholder byte. */
    constexpr uint8_t MMC_COMMAND_DEFAULT = 0x00;

    /** @brief NRPN parameter MSB controller number. */
    constexpr uint8_t NRPN_MSB_CONTROLLER = 99;

    /** @brief NRPN parameter LSB controller number. */
    constexpr uint8_t NRPN_LSB_CONTROLLER = 98;

    /** @brief Data Entry MSB controller number. */
    constexpr uint8_t DATA_ENTRY_MSB_CONTROLLER = 6;

    /** @brief Data Entry LSB controller number. */
    constexpr uint8_t DATA_ENTRY_LSB_CONTROLLER = 38;

    /** @brief Controller-number offset to access paired LSB CC for 14-bit CC. */
    constexpr uint8_t CONTROL_CHANGE_LSB_OFFSET = 32;

    /** @brief Maximum SysEx7 payload bytes carried by one Data64 UMP. */
    constexpr inline size_t SYSEX7_DATA64_MAX_PAYLOAD_SIZE = 6;

    /** @brief UMP message-type field bit shift. */
    constexpr inline uint8_t UMP_MESSAGE_TYPE_SHIFT = 28;

    /** @brief UMP group field bit shift. */
    constexpr inline uint8_t UMP_GROUP_SHIFT = 24;

    /** @brief UMP 4-bit status/command field bit shift. */
    constexpr inline uint8_t UMP_STATUS_NIBBLE_SHIFT = 20;

    /** @brief UMP MIDI status byte / channel field bit shift. */
    constexpr inline uint8_t UMP_STATUS_BYTE_SHIFT = 16;

    /** @brief UMP first payload byte bit shift. */
    constexpr inline uint8_t UMP_PAYLOAD_BYTE_1_SHIFT = 8;

    /** @brief MIDI 1 status-byte message type bit shift. */
    constexpr inline uint8_t MIDI_STATUS_TYPE_SHIFT = 4;

    /** @brief Number of SysEx7 payload bytes carried in the first Data64 word. */
    constexpr inline size_t SYSEX7_DATA64_FIRST_WORD_PAYLOAD_SIZE = 2;

    /** @brief Last SysEx7 payload byte index carried in the second Data64 word. */
    constexpr inline size_t SYSEX7_DATA64_SECOND_WORD_LAST_PAYLOAD_INDEX = SYSEX7_DATA64_MAX_PAYLOAD_SIZE - 1;

    /**
     * @brief MIDI message/event types.
     */
    enum class MessageType : uint8_t
    {
        NoteOff                       = 0x80,
        NoteOn                        = 0x90,
        ControlChange                 = 0xB0,
        ProgramChange                 = 0xC0,
        AfterTouchChannel             = 0xD0,
        AfterTouchPoly                = 0xA0,
        PitchBend                     = 0xE0,
        SysEx                         = 0xF0,
        SysCommonTimeCodeQuarterFrame = 0xF1,
        SysCommonSongPosition         = 0xF2,
        SysCommonSongSelect           = 0xF3,
        SysCommonTuneRequest          = 0xF6,
        SysRealTimeClock              = 0xF8,
        SysRealTimeStart              = 0xFA,
        SysRealTimeContinue           = 0xFB,
        SysRealTimeStop               = 0xFC,
        SysRealTimeActiveSensing      = 0xFE,
        SysRealTimeSystemReset        = 0xFF,
        MmcPlay                       = 0x02,
        MmcStop                       = 0x01,
        MmcPause                      = 0x09,
        MmcRecordStart                = 0x06,
        MmcRecordStop                 = 0x07,
        Invalid                       = 0x00,
    };

    /**
     * @brief Chromatic note enumeration.
     */
    enum class Note : uint8_t
    {
        C,
        CSharp,
        D,
        DSharp,
        E,
        F,
        FSharp,
        G,
        GSharp,
        A,
        ASharp,
        B,
        Count,
    };

    /**
     * @brief UMP SysEx7 packet status values.
     */
    enum class SysEx7Status : uint8_t
    {
        Complete = 0x00,
        Start    = 0x01,
        Continue = 0x02,
        End      = 0x03,
    };

    /**
     * @brief Converts 2x7-bit bytes into a 14-bit value.
     */
    class Merge14Bit
    {
        public:
        /**
         * @brief Constructs a merged 14-bit value from two 7-bit bytes.
         *
         * @param high High 7-bit part.
         * @param low Low 7-bit part.
         */
        constexpr explicit Merge14Bit(uint8_t high, uint8_t low)
        {
            if (high & 0x01)
            {
                low |= (1 << (BITS_IN_BYTE - 1));
            }
            else
            {
                low &= ~(1 << (BITS_IN_BYTE - 1));
            }

            high >>= 1;

            uint16_t joined = high;
            joined <<= BITS_IN_BYTE;
            joined |= low;

            _value = joined;
        }

        /**
         * @brief Returns the merged 14-bit value.
         *
         * @return Merged value in range `[0, 16383]`.
         */
        constexpr uint16_t value() const
        {
            return _value;
        }

        /**
         * @brief Returns the merged 14-bit value.
         *
         * @return Merged value in range `[0, 16383]`.
         */
        constexpr uint16_t operator()() const
        {
            return value();
        }

        private:
        uint16_t _value = 0;
    };

    /**
     * @brief Splits a 14-bit value into 2x7-bit bytes.
     */
    class Split14Bit
    {
        public:
        /**
         * @brief Splits a 14-bit value into two 7-bit bytes.
         *
         * @param value Input value in range `[0, 16383]`.
         */
        constexpr explicit Split14Bit(uint16_t value)
        {
            constexpr uint16_t BYTE_MASK = 0xFF;

            uint8_t new_high = (value >> BITS_IN_BYTE) & BYTE_MASK;
            uint8_t new_low  = value & BYTE_MASK;
            new_high         = (new_high << 1) & MIDI_DATA_BYTE_MASK;

            if ((new_low >> (BITS_IN_BYTE - 1)) & 0x01)
            {
                new_high |= 0x01;
            }
            else
            {
                new_high &= ~0x01;
            }

            new_low &= MIDI_DATA_BYTE_MASK;
            _high = new_high;
            _low  = new_low;
        }

        /**
         * @brief Returns the high 7-bit byte.
         */
        constexpr uint8_t high() const
        {
            return _high;
        }

        /**
         * @brief Returns the low 7-bit byte.
         */
        constexpr uint8_t low() const
        {
            return _low;
        }

        private:
        uint8_t _high = 0;
        uint8_t _low  = 0;
    };

    /**
     * @brief Calculates octave index from raw MIDI note.
     *
     * @param note Raw MIDI note value (0-127).
     *
     * @return Octave index.
     */
    constexpr uint8_t note_to_octave(int8_t note)
    {
        auto sanitized_note = note & static_cast<int8_t>(MIDI_DATA_BYTE_MASK);

        return static_cast<uint8_t>(sanitized_note) / static_cast<uint8_t>(Note::Count);
    }

    /**
     * @brief Calculates tonic (root note) from raw MIDI note.
     *
     * @param note Raw MIDI note value (0-127).
     *
     * @return Note tonic.
     */
    constexpr Note note_to_tonic(int8_t note)
    {
        auto sanitized_note = note & static_cast<int8_t>(MIDI_DATA_BYTE_MASK);

        return static_cast<Note>(static_cast<uint8_t>(sanitized_note) % static_cast<uint8_t>(Note::Count));
    }

    /**
     * @brief Extracts MIDI channel from a status byte.
     *
     * @param status MIDI status byte.
     *
     * @return MIDI channel in range [1, 16].
     */
    constexpr uint8_t channel_from_status_byte(uint8_t status)
    {
        return (status & MIDI_CHANNEL_NIBBLE_FROM_STATUS_MASK) + 1;
    }

    /**
     * @brief Returns whether type is a channel message.
     *
     * @param type MIDI message type.
     *
     * @return `true` if @p type is a channel message, otherwise `false`.
     */
    constexpr bool is_channel_message(MessageType type)
    {
        return (type == MessageType::NoteOff ||
                type == MessageType::NoteOn ||
                type == MessageType::ControlChange ||
                type == MessageType::AfterTouchPoly ||
                type == MessageType::AfterTouchChannel ||
                type == MessageType::PitchBend ||
                type == MessageType::ProgramChange);
    }

    /**
     * @brief Returns whether type is a system real-time message.
     *
     * @param type MIDI message type.
     *
     * @return `true` if @p type is a system real-time message, otherwise `false`.
     */
    constexpr bool is_system_real_time(MessageType type)
    {
        return (type == MessageType::SysRealTimeClock ||
                type == MessageType::SysRealTimeStart ||
                type == MessageType::SysRealTimeContinue ||
                type == MessageType::SysRealTimeStop ||
                type == MessageType::SysRealTimeActiveSensing ||
                type == MessageType::SysRealTimeSystemReset);
    }

    /**
     * @brief Returns whether type is a system common message.
     *
     * @param type MIDI message type.
     *
     * @return `true` if @p type is a system common message, otherwise `false`.
     */
    constexpr bool is_system_common(MessageType type)
    {
        return (type == MessageType::SysCommonTimeCodeQuarterFrame ||
                type == MessageType::SysCommonSongPosition ||
                type == MessageType::SysCommonSongSelect ||
                type == MessageType::SysCommonTuneRequest);
    }

    /**
     * @brief Extracts MIDI message type from status byte.
     *
     * @param status MIDI status byte.
     *
     * @return Decoded MIDI message type, or `MessageType::Invalid` for invalid status bytes.
     */
    constexpr MessageType type_from_status_byte(uint8_t status)
    {
        if ((status < MIDI_STATUS_MIN) ||
            (status == MIDI_STATUS_UNDEFINED_0) ||
            (status == MIDI_STATUS_UNDEFINED_1) ||
            (status == MIDI_STATUS_UNDEFINED_2) ||
            (status == MIDI_STATUS_UNDEFINED_3))
        {
            return MessageType::Invalid;
        }

        if (status < MIDI_SYSTEM_MESSAGE_MIN)
        {
            return static_cast<MessageType>(status & MIDI_STATUS_CHANNEL_MASK);
        }

        switch (status)
        {
        case static_cast<uint8_t>(MessageType::SysEx):
        case static_cast<uint8_t>(MessageType::SysCommonTimeCodeQuarterFrame):
        case static_cast<uint8_t>(MessageType::SysCommonSongPosition):
        case static_cast<uint8_t>(MessageType::SysCommonSongSelect):
        case static_cast<uint8_t>(MessageType::SysCommonTuneRequest):
        case static_cast<uint8_t>(MessageType::SysRealTimeClock):
        case static_cast<uint8_t>(MessageType::SysRealTimeStart):
        case static_cast<uint8_t>(MessageType::SysRealTimeContinue):
        case static_cast<uint8_t>(MessageType::SysRealTimeStop):
        case static_cast<uint8_t>(MessageType::SysRealTimeActiveSensing):
        case static_cast<uint8_t>(MessageType::SysRealTimeSystemReset):
            return static_cast<MessageType>(status);

        default:
            return MessageType::Invalid;
        }
    }

    /**
     * @brief Builds the first word of a UMP SysEx7 Data64 packet.
     *
     * First 32-bit word layout for a 64-bit SysEx7/Data64 UMP:
     * - bits 31..28: Message Type (`UMP_MT_DATA_64`)
     * - bits 27..24: UMP group
     * - bits 23..20: SysEx7 status (`Complete`, `Start`, `Continue`, `End`)
     * - bits 19..16: payload byte count (0..6)
     * - bits 15..0: first two SysEx7 payload bytes
     *
     * @param group UMP group number.
     * @param status SysEx7 packet status.
     * @param size Number of SysEx7 payload bytes carried in this packet.
     *
     * @return Encoded first UMP word.
     */
    constexpr uint32_t sysex7_header(uint8_t group, SysEx7Status status, size_t size)
    {
        return (UMP_MT_DATA_64 << UMP_MESSAGE_TYPE_SHIFT) |
               ((group & MIDI_LOW_NIBBLE_MASK) << UMP_GROUP_SHIFT) |
               ((static_cast<uint8_t>(status) & MIDI_LOW_NIBBLE_MASK) << UMP_STATUS_NIBBLE_SHIFT) |
               ((size & MIDI_LOW_NIBBLE_MASK) << UMP_STATUS_BYTE_SHIFT);
    }

    /**
     * @brief Extracts SysEx7 status from a Data64 UMP.
     *
     * @param packet Packet to read.
     *
     * @return SysEx7 packet status.
     */
    constexpr SysEx7Status sysex7_status(const midi_ump& packet)
    {
        return static_cast<SysEx7Status>((packet.data[0] >> UMP_STATUS_NIBBLE_SHIFT) & MIDI_LOW_NIBBLE_MASK);
    }

    /**
     * @brief Extracts SysEx7 payload byte count from a Data64 UMP.
     *
     * @param packet Packet to read.
     *
     * @return Number of SysEx7 payload bytes carried in this packet.
     */
    constexpr uint8_t sysex7_payload_size(const midi_ump& packet)
    {
        return (packet.data[0] >> UMP_STATUS_BYTE_SHIFT) & MIDI_LOW_NIBBLE_MASK;
    }

    /**
     * @brief Returns whether a SysEx7 status starts a SysEx message.
     *
     * @param status SysEx7 packet status.
     *
     * @return `true` for `Complete` and `Start`, otherwise `false`.
     */
    constexpr bool sysex7_starts_message(SysEx7Status status)
    {
        return (status == SysEx7Status::Complete) || (status == SysEx7Status::Start);
    }

    /**
     * @brief Returns whether a SysEx7 status ends a SysEx message.
     *
     * @param status SysEx7 packet status.
     *
     * @return `true` for `Complete` and `End`, otherwise `false`.
     */
    constexpr bool sysex7_ends_message(SysEx7Status status)
    {
        return (status == SysEx7Status::Complete) || (status == SysEx7Status::End);
    }

    /**
     * @brief Returns whether a packet is a valid SysEx7/Data64 UMP.
     *
     * @param packet Packet to validate.
     *
     * @return `true` when packet type, status, and payload size are valid.
     */
    constexpr bool is_sysex7_packet(const midi_ump& packet)
    {
        return (UMP_MT(packet) == UMP_MT_DATA_64) &&
               (sysex7_payload_size(packet) <= SYSEX7_DATA64_MAX_PAYLOAD_SIZE) &&
               (static_cast<uint8_t>(sysex7_status(packet)) <= static_cast<uint8_t>(SysEx7Status::End));
    }

    /**
     * @brief Extracts MIDI message type from one UMP packet.
     *
     * @param packet UMP packet.
     *
     * @return Decoded MIDI message type, or `MessageType::Invalid` when packet type is unsupported.
     */
    constexpr MessageType type(const midi_ump& packet)
    {
        if (UMP_MT(packet) == UMP_MT_MIDI1_CHANNEL_VOICE)
        {
            return static_cast<MessageType>(UMP_MIDI_COMMAND(packet) << MIDI_STATUS_TYPE_SHIFT);
        }

        if (UMP_MT(packet) == UMP_MT_SYS_RT_COMMON)
        {
            return type_from_status_byte(UMP_MIDI_STATUS(packet));
        }

        if (is_sysex7_packet(packet))
        {
            return MessageType::SysEx;
        }

        return MessageType::Invalid;
    }

    /**
     * @brief Appends one SysEx7 payload byte to a Data64 UMP.
     *
     * @param packet Packet to update.
     * @param index SysEx7 payload byte index in range `[0, 5]`.
     * @param data SysEx7 payload byte.
     */
    inline void write_sysex7_payload_byte(midi_ump& packet, size_t index, uint8_t data)
    {
        const uint8_t BYTE = data & MIDI_DATA_BYTE_MASK;

        if (index < SYSEX7_DATA64_FIRST_WORD_PAYLOAD_SIZE)
        {
            packet.data[0] |= static_cast<uint32_t>(BYTE) << (BITS_IN_BYTE * ((SYSEX7_DATA64_FIRST_WORD_PAYLOAD_SIZE - 1) - index));
        }
        else
        {
            packet.data[1] |= static_cast<uint32_t>(BYTE) << (BITS_IN_BYTE * (SYSEX7_DATA64_SECOND_WORD_LAST_PAYLOAD_INDEX - index));
        }
    }

    /**
     * @brief Reads one SysEx7 payload byte from a Data64 UMP.
     *
     * @param packet Packet to read.
     * @param index SysEx7 payload byte index in range `[0, 5]`.
     *
     * @return SysEx7 payload byte.
     */
    constexpr uint8_t read_sysex7_payload_byte(const midi_ump& packet, size_t index)
    {
        if (index < SYSEX7_DATA64_FIRST_WORD_PAYLOAD_SIZE)
        {
            return (packet.data[0] >> (BITS_IN_BYTE * ((SYSEX7_DATA64_FIRST_WORD_PAYLOAD_SIZE - 1) - index))) & MIDI_DATA_BYTE_MASK;
        }

        return (packet.data[1] >> (BITS_IN_BYTE * (SYSEX7_DATA64_SECOND_WORD_LAST_PAYLOAD_INDEX - index))) & MIDI_DATA_BYTE_MASK;
    }

    /**
     * @brief Builds a complete SysEx7 Data64 UMP with up to six payload bytes.
     *
     * @param group UMP group number.
     * @param payload SysEx7 payload bytes, excluding MIDI 1 SysEx start/end bytes.
     *
     * @return Encoded UMP on success, otherwise `std::nullopt` when payload is too large.
     */
    inline std::optional<midi_ump> sysex7_complete_ump(uint8_t group, std::span<const uint8_t> payload)
    {
        if (payload.size() > SYSEX7_DATA64_MAX_PAYLOAD_SIZE)
        {
            return {};
        }

        midi_ump packet = {};
        packet.data[0]  = sysex7_header(group, SysEx7Status::Complete, payload.size());

        for (size_t i = 0; i < payload.size(); i++)
        {
            /*
             * SysEx7/Data64 uses `data[0]` for the header plus payload bytes
             * 0..1, and `data[1]` for payload bytes 2..5. `data[2]` and
             * `data[3]` are unused.
             */
            write_sysex7_payload_byte(packet, i, payload[i]);
        }

        return packet;
    }

    /**
     * @brief Builds a MIDI 1 channel voice UMP from one MIDI 1 status byte.
     *
     * @param group UMP group number.
     * @param status MIDI 1 status byte containing command and channel.
     * @param data1 First MIDI 1 data byte.
     * @param data2 Second MIDI 1 data byte.
     *
     * @return Encoded MIDI 1 channel voice UMP.
     */
    constexpr midi_ump midi1_channel_voice_ump(uint8_t group, uint8_t status, uint8_t data1, uint8_t data2)
    {
        return {
            {
                (static_cast<uint32_t>(UMP_MT_MIDI1_CHANNEL_VOICE) << UMP_MESSAGE_TYPE_SHIFT) |
                    (static_cast<uint32_t>(group & MIDI_LOW_NIBBLE_MASK) << UMP_GROUP_SHIFT) |
                    (static_cast<uint32_t>((status & MIDI_STATUS_CHANNEL_MASK) >> MIDI_STATUS_TYPE_SHIFT) << UMP_STATUS_NIBBLE_SHIFT) |
                    (static_cast<uint32_t>(status & MIDI_LOW_NIBBLE_MASK) << UMP_STATUS_BYTE_SHIFT) |
                    (static_cast<uint32_t>(data1 & MIDI_DATA_BYTE_MASK) << UMP_PAYLOAD_BYTE_1_SHIFT) |
                    static_cast<uint32_t>(data2 & MIDI_DATA_BYTE_MASK),
            },
        };
    }

    /**
     * @brief Builds a system realtime/common UMP from one MIDI 1 status byte.
     *
     * @param group UMP group number.
     * @param status MIDI 1 system status byte.
     * @param data1 First MIDI 1 data byte, when applicable.
     * @param data2 Second MIDI 1 data byte, when applicable.
     *
     * @return Encoded system realtime/common UMP.
     */
    constexpr midi_ump system_ump(uint8_t group, uint8_t status, uint8_t data1, uint8_t data2)
    {
        return {
            {
                (static_cast<uint32_t>(UMP_MT_SYS_RT_COMMON) << UMP_MESSAGE_TYPE_SHIFT) |
                    (static_cast<uint32_t>(group & MIDI_LOW_NIBBLE_MASK) << UMP_GROUP_SHIFT) |
                    (static_cast<uint32_t>(status) << UMP_STATUS_BYTE_SHIFT) |
                    (static_cast<uint32_t>(data1 & MIDI_DATA_BYTE_MASK) << UMP_PAYLOAD_BYTE_1_SHIFT) |
                    static_cast<uint32_t>(data2 & MIDI_DATA_BYTE_MASK),
            },
        };
    }

    /**
     * @brief Callable concept for writing one MIDI 1 byte.
     */
    template<typename WriteByte>
    concept Midi1ByteWriter = requires(WriteByte write_byte, uint8_t data) {
        { write_byte(data) } -> std::convertible_to<bool>;
    };

    /**
     * @brief Callable concept for writing one UMP packet.
     */
    template<typename WritePacket>
    concept UmpPacketWriter = requires(WritePacket write_packet, const midi_ump& packet) {
        { write_packet(packet) } -> std::convertible_to<bool>;
    };

    /**
     * @brief Callable concept for reading one MIDI 1 byte.
     */
    template<typename ReadByte>
    concept Midi1ByteReader = requires(ReadByte read_byte) {
        { read_byte() } -> std::same_as<std::optional<uint8_t>>;
    };

    /**
     * @brief Writes the MIDI 1 byte-stream representation of one UMP.
     *
     * @param packet UMP to serialize.
     * @param group Expected UMP group.
     * @param write_byte Callable that writes one MIDI 1 byte and returns `true` on success.
     *
     * @tparam WriteByte Callable type satisfying `Midi1ByteWriter`.
     *
     * @return `true` when all bytes were written, otherwise `false`.
     */
    template<Midi1ByteWriter WriteByte>
    bool write_ump_as_midi1_bytes(const midi_ump& packet, uint8_t group, WriteByte write_byte)
    {
        if (UMP_GROUP(packet) != group)
        {
            return false;
        }

        switch (UMP_MT(packet))
        {
        case UMP_MT_MIDI1_CHANNEL_VOICE:
        {
            const uint8_t STATUS = UMP_MIDI_STATUS(packet);

            if (!write_byte(STATUS) || !write_byte(UMP_MIDI1_P1(packet)))
            {
                return false;
            }

            const MessageType TYPE = type_from_status_byte(STATUS);

            if ((TYPE != MessageType::ProgramChange) && (TYPE != MessageType::AfterTouchChannel))
            {
                return write_byte(UMP_MIDI1_P2(packet));
            }

            return true;
        }

        case UMP_MT_SYS_RT_COMMON:
        {
            const uint8_t STATUS = UMP_MIDI_STATUS(packet);

            if (!write_byte(STATUS))
            {
                return false;
            }

            switch (type_from_status_byte(STATUS))
            {
            case MessageType::SysCommonTimeCodeQuarterFrame:
            case MessageType::SysCommonSongSelect:
                return write_byte(UMP_MIDI1_P1(packet));

            case MessageType::SysCommonSongPosition:
                return write_byte(UMP_MIDI1_P1(packet)) && write_byte(UMP_MIDI1_P2(packet));

            default:
                return true;
            }
        }

        case UMP_MT_DATA_64:
        {
            if (!is_sysex7_packet(packet))
            {
                return false;
            }

            const auto    STATUS = sysex7_status(packet);
            const uint8_t SIZE   = sysex7_payload_size(packet);

            if (sysex7_starts_message(STATUS))
            {
                if (!write_byte(SYS_EX_START))
                {
                    return false;
                }
            }

            for (uint8_t i = 0; i < SIZE; i++)
            {
                const uint8_t BYTE = read_sysex7_payload_byte(packet, i);

                if (!write_byte(BYTE))
                {
                    return false;
                }
            }

            if (sysex7_ends_message(STATUS))
            {
                return write_byte(SYS_EX_END);
            }

            return true;
        }

        default:
            return false;
        }
    }

    /**
     * @brief Writes one SysEx7 payload as one or more Data64 UMP packets.
     *
     * @param group UMP group number assigned to emitted packets.
     * @param payload SysEx7 payload bytes, excluding MIDI 1 SysEx start/end bytes.
     * @param write_packet Callable that writes one UMP packet and returns `true` on success.
     *
     * @tparam WritePacket Callable type satisfying `UmpPacketWriter`.
     *
     * @return `true` when all packets were written, otherwise `false`.
     */
    template<UmpPacketWriter WritePacket>
    bool write_sysex7_payload_as_ump_packets(uint8_t group, std::span<const uint8_t> payload, WritePacket write_packet)
    {
        size_t offset = 0;

        do
        {
            const size_t REMAINING_PAYLOAD = payload.size() - offset;
            const size_t CHUNK_SIZE        = std::min(REMAINING_PAYLOAD, SYSEX7_DATA64_MAX_PAYLOAD_SIZE);
            const bool   FIRST_CHUNK       = (offset == 0);
            const bool   LAST_CHUNK        = ((offset + CHUNK_SIZE) == payload.size());

            SysEx7Status status = SysEx7Status::Continue;

            if (FIRST_CHUNK && LAST_CHUNK)
            {
                status = SysEx7Status::Complete;
            }
            else if (FIRST_CHUNK)
            {
                status = SysEx7Status::Start;
            }
            else if (LAST_CHUNK)
            {
                status = SysEx7Status::End;
            }

            midi_ump packet = {};
            packet.data[0]  = sysex7_header(group, status, CHUNK_SIZE);

            for (size_t i = 0; i < CHUNK_SIZE; i++)
            {
                write_sysex7_payload_byte(packet, i, payload[offset + i]);
            }

            if (!write_packet(packet))
            {
                return false;
            }

            offset += CHUNK_SIZE;
        } while (offset < payload.size() || payload.empty());

        return true;
    }

    /**
     * @brief Stateful MIDI 1 byte-stream to UMP parser.
     */
    class Midi1ByteToUmpParser
    {
        public:
        /**
         * @brief Resets pending parser state.
         */
        void reset()
        {
            _running_status     = 0;
            _pending            = {};
            _pending_index      = 0;
            _expected_size      = 0;
            _sysex_buffer       = {};
            _sysex_size         = 0;
            _sysex_active       = false;
            _sysex_multi_packet = false;
        }

        /**
         * @brief Parses one MIDI 1 byte.
         *
         * @param data MIDI 1 byte.
         * @param group UMP group assigned to produced packets.
         *
         * @return Produced UMP when a full message is available.
         */
        std::optional<midi_ump> parse(uint8_t data, uint8_t group)
        {
            if (data == SYS_EX_START)
            {
                _pending_index      = 0;
                _expected_size      = 0;
                _running_status     = 0;
                _sysex_size         = 0;
                _sysex_active       = true;
                _sysex_multi_packet = false;
                return {};
            }

            if (_sysex_active)
            {
                return parse_sysex_byte(data, group);
            }

            if (_pending_index == 0)
            {
                if ((data < MIDI_STATUS_MIN) && is_channel_message(type_from_status_byte(_running_status)))
                {
                    _pending.at(0) = _running_status;
                    _pending.at(1) = data;
                    _pending_index = 2;
                }
                else
                {
                    _pending.at(0) = data;
                    _pending_index = 1;
                }

                const auto TYPE = type_from_status_byte(_pending.at(0));

                switch (TYPE)
                {
                case MessageType::SysRealTimeStart:
                case MessageType::SysRealTimeContinue:
                case MessageType::SysRealTimeStop:
                case MessageType::SysRealTimeClock:
                case MessageType::SysRealTimeActiveSensing:
                case MessageType::SysRealTimeSystemReset:
                case MessageType::SysCommonTuneRequest:
                {
                    _pending_index = 0;
                    _expected_size = 0;
                    return system_ump(group, static_cast<uint8_t>(TYPE), 0, 0);
                }
                break;

                case MessageType::ProgramChange:
                case MessageType::AfterTouchChannel:
                case MessageType::SysCommonTimeCodeQuarterFrame:
                case MessageType::SysCommonSongSelect:
                {
                    _expected_size = 2;
                }
                break;

                case MessageType::NoteOn:
                case MessageType::NoteOff:
                case MessageType::ControlChange:
                case MessageType::PitchBend:
                case MessageType::AfterTouchPoly:
                case MessageType::SysCommonSongPosition:
                {
                    _expected_size = 3;
                }
                break;

                default:
                {
                    reset();
                    return {};
                }
                break;
                }

                if (_pending_index >= _expected_size)
                {
                    return complete_message(group);
                }

                return {};
            }

            if (data >= MIDI_STATUS_MIN)
            {
                reset();
                return parse(data, group);
            }

            _pending.at(_pending_index++) = data;

            if (_pending_index >= _expected_size)
            {
                return complete_message(group);
            }

            return {};
        }

        private:
        std::array<uint8_t, 3>                              _pending            = {};
        uint8_t                                             _running_status     = 0;
        size_t                                              _pending_index      = 0;
        size_t                                              _expected_size      = 0;
        std::array<uint8_t, SYSEX7_DATA64_MAX_PAYLOAD_SIZE> _sysex_buffer       = {};
        size_t                                              _sysex_size         = 0;
        bool                                                _sysex_active       = false;
        bool                                                _sysex_multi_packet = false;

        std::optional<midi_ump> parse_sysex_byte(uint8_t data, uint8_t group)
        {
            if (data == SYS_EX_END)
            {
                const auto STATUS = _sysex_multi_packet ? SysEx7Status::End : SysEx7Status::Complete;
                _sysex_active     = false;
                return flush_sysex(group, STATUS);
            }

            if (data >= MIDI_STATUS_MIN)
            {
                reset();
                return parse(data, group);
            }

            if (_sysex_size == _sysex_buffer.size())
            {
                auto packet                     = flush_sysex(group, _sysex_multi_packet ? SysEx7Status::Continue : SysEx7Status::Start);
                _sysex_multi_packet             = true;
                _sysex_buffer.at(_sysex_size++) = data & MIDI_DATA_BYTE_MASK;
                return packet;
            }

            _sysex_buffer.at(_sysex_size++) = data & MIDI_DATA_BYTE_MASK;
            return {};
        }

        std::optional<midi_ump> flush_sysex(uint8_t group, SysEx7Status status)
        {
            midi_ump packet = {};
            packet.data[0]  = sysex7_header(group, status, _sysex_size);

            for (size_t i = 0; i < _sysex_size; i++)
            {
                write_sysex7_payload_byte(packet, i, _sysex_buffer.at(i));
            }

            _sysex_size = 0;
            return packet;
        }

        std::optional<midi_ump> complete_message(uint8_t group)
        {
            const uint8_t STATUS = _pending.at(0);
            const uint8_t DATA_1 = (_expected_size > 1) ? _pending.at(1) : 0;
            const uint8_t DATA_2 = (_expected_size > 2) ? _pending.at(2) : 0;

            _pending_index = 0;
            _expected_size = 0;

            if (STATUS < MIDI_SYSTEM_MESSAGE_MIN)
            {
                _running_status = STATUS;
                return midi1_channel_voice_ump(group, STATUS, DATA_1, DATA_2);
            }

            _running_status = 0;
            return system_ump(group, STATUS, DATA_1, DATA_2);
        }
    };

    /**
     * @brief Reads MIDI 1 bytes until the parser produces one UMP.
     *
     * @param parser Stateful MIDI 1 byte-stream parser.
     * @param group UMP group assigned to produced packets.
     * @param read_byte Callable that reads one MIDI 1 byte.
     *
     * @tparam ReadByte Callable type satisfying `Midi1ByteReader`.
     *
     * @return Produced UMP, or `std::nullopt` when no byte is available.
     */
    template<Midi1ByteReader ReadByte>
    std::optional<midi_ump> read_midi1_bytes_as_ump(Midi1ByteToUmpParser& parser, uint8_t group, ReadByte read_byte)
    {
        while (true)
        {
            auto data = read_byte();

            if (!data.has_value())
            {
                return {};
            }

            auto packet = parser.parse(data.value(), group);

            if (packet.has_value())
            {
                return packet;
            }
        }
    }
}    // namespace zlibs::utils::midi
