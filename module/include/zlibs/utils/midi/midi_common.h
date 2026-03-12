/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace zlibs::utils::midi
{
    /** @brief Number of bits in one byte. */
    constexpr inline uint8_t BITS_IN_BYTE = 8;

    /** @brief Mask for valid 7-bit MIDI data-byte payload. */
    constexpr inline uint8_t MIDI_DATA_BYTE_MASK = 0x7F;

    /** @brief Mask for extracting channel nibble from status byte. */
    constexpr inline uint8_t MIDI_STATUS_NIBBLE_MASK = 0x0F;

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

    /** @brief Lowest valid one-based MIDI channel number. */
    constexpr uint8_t MIDI_CHANNEL_MIN = 1;

    /** @brief Highest valid one-based MIDI channel number. */
    constexpr uint8_t MIDI_CHANNEL_MAX = 16;

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

    /** @brief Mask for 13-bit BLE-MIDI timestamp field. */
    constexpr uint16_t BLE_TIMESTAMP_MASK = 0x1FFF;

    /** @brief Bit shift for BLE-MIDI high timestamp bits. */
    constexpr uint8_t BLE_TIMESTAMP_HIGH_SHIFT = 7;

    /** @brief Mask for BLE-MIDI 7-bit timestamp/data payload. */
    constexpr uint8_t BLE_DATA_MASK = 0x7F;

    /** @brief USB-MIDI event-code increment step used for SysEx end packet sizing. */
    constexpr uint8_t SYS_EX_USB_EVENT_STEP = 0x10;

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
        Nrpn7bit                      = 0x99,
        Nrpn14bit                     = 0x38,
        ControlChange14bit            = 0x32,
        Invalid                       = 0x00,
    };

    /**
     * @brief Note-off transmission mode.
     */
    enum class NoteOffType : uint8_t
    {
        NoteOnZeroVelocity,
        StandardNoteOff,
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
     * @brief Decoded MIDI message payload.
     */
    struct Message
    {
        uint8_t     channel                                                = 1;
        MessageType type                                                   = MessageType::Invalid;
        uint8_t     data1                                                  = 0;
        uint8_t     data2                                                  = 0;
        uint8_t     sys_ex_array[CONFIG_ZLIBS_UTILS_MIDI_SYSEX_ARRAY_SIZE] = {};
        bool        valid                                                  = false;
        size_t      length                                                 = 0;
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
        explicit Merge14Bit(uint8_t high, uint8_t low)
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
        uint16_t value() const
        {
            return _value;
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
        explicit Split14Bit(uint16_t value)
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
        uint8_t high() const
        {
            return _high;
        }

        /**
         * @brief Returns the low 7-bit byte.
         */
        uint8_t low() const
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
        return (status & MIDI_STATUS_NIBBLE_MASK) + 1;
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
     * @brief Interface for MIDI thru output sinks.
     */
    class Thru
    {
        public:
        virtual ~Thru() = default;

        /**
         * @brief Begins transmission of one MIDI message.
         *
         * @param type MIDI message type to transmit.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool begin_transmission(MessageType type) = 0;

        /**
         * @brief Writes one MIDI byte to the active transmission.
         *
         * @param data MIDI byte to write.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool write(uint8_t data) = 0;

        /**
         * @brief Finalizes transmission of the active MIDI message.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool end_transmission() = 0;
    };

    /**
     * @brief Transport interface used by `midi::Base` for RX/TX.
     */
    class Transport : public Thru
    {
        public:
        ~Transport() override = default;

        /**
         * @brief Initializes the transport backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init() = 0;

        /**
         * @brief Deinitializes the transport backend.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool deinit() = 0;

        /**
         * @brief Reads one byte from the transport backend.
         *
         * @return Read byte on success, or `std::nullopt` when no byte is available.
         */
        virtual std::optional<uint8_t> read() = 0;
    };
}    // namespace zlibs::utils::midi
