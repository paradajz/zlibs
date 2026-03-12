/*
 * Copyright (c) 2016 Francois Best
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "midi_common.h"

#include "zlibs/utils/misc/mutex.h"

#include <span>

namespace zlibs::utils::midi
{
    /**
     * @brief Core transport-agnostic MIDI parser and transmitter.
     *
     * All `send_*` member functions are thread-safe.
     */
    class Base
    {
        public:
        /**
         * @brief Constructs a MIDI instance over a transport backend.
         *
         * @param transport Transport implementation.
         */
        explicit Base(Transport& transport)
            : _transport(transport)
        {}

        /**
         * @brief Initializes MIDI parser/transmitter and underlying transport.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init();

        /**
         * @brief Deinitializes MIDI parser/transmitter and underlying transport.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool deinit();

        /**
         * @brief Returns whether this instance is initialized.
         *
         * @return `true` when initialized, otherwise `false`.
         */
        bool initialized();

        /**
         * @brief Resets internal parser state.
         */
        void reset();

        /**
         * @brief Returns underlying transport interface.
         *
         * @return Transport reference.
         */
        Transport& transport();

        /**
         * @brief Sends Note On message.
         *
         * @param note_number MIDI note number.
         * @param velocity Note velocity.
         * @param channel MIDI channel (1-16).
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_note_on(uint8_t note_number, uint8_t velocity, uint8_t channel);

        /**
         * @brief Sends Note Off message according to current note-off mode.
         *
         * @param note_number MIDI note number.
         * @param velocity Note release velocity.
         * @param channel MIDI channel (1-16).
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_note_off(uint8_t note_number, uint8_t velocity, uint8_t channel);

        /**
         * @brief Sends Program Change message.
         *
         * @param program_number Program number.
         * @param channel MIDI channel (1-16).
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_program_change(uint8_t program_number, uint8_t channel);

        /**
         * @brief Sends Control Change message.
         *
         * @param control_number Controller number.
         * @param control_value Controller value.
         * @param channel MIDI channel (1-16).
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_control_change(uint8_t control_number, uint8_t control_value, uint8_t channel);

        /**
         * @brief Sends 14-bit Control Change using coarse/fine controllers.
         *
         * @param control_number Controller number.
         * @param control_value 14-bit controller value.
         * @param channel MIDI channel (1-16).
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_control_change_14bit(uint16_t control_number, uint16_t control_value, uint8_t channel);

        /**
         * @brief Sends Pitch Bend message.
         *
         * @param pitch_value 14-bit pitch bend value.
         * @param channel MIDI channel (1-16).
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_pitch_bend(uint16_t pitch_value, uint8_t channel);

        /**
         * @brief Sends Polyphonic AfterTouch message.
         *
         * @param pressure AfterTouch pressure.
         * @param channel MIDI channel (1-16).
         * @param note_number Target MIDI note number.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_after_touch(uint8_t pressure, uint8_t channel, uint8_t note_number);

        /**
         * @brief Sends Channel (monophonic) AfterTouch message.
         *
         * @param pressure AfterTouch pressure.
         * @param channel MIDI channel (1-16).
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_after_touch(uint8_t pressure, uint8_t channel);

        /**
         * @brief Sends System Exclusive message.
         *
         * @param data SysEx payload bytes.
         * @param array_contains_boundaries Whether start/end bytes are included in @p data.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_sys_ex(std::span<const uint8_t> data, bool array_contains_boundaries);

        /**
         * @brief Sends MIDI time-code quarter frame from nibbles.
         *
         * @param type_nibble Quarter-frame type nibble.
         * @param values_nibble Quarter-frame value nibble.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_time_code_quarter_frame(uint8_t type_nibble, uint8_t values_nibble);

        /**
         * @brief Sends MIDI time-code quarter frame raw byte.
         *
         * @param data Raw quarter-frame byte.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_time_code_quarter_frame(uint8_t data);

        /**
         * @brief Sends Song Position Pointer message.
         *
         * @param beats Beat position value.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_song_position(uint16_t beats);

        /**
         * @brief Sends Song Select message.
         *
         * @param song_number Song index.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_song_select(uint8_t song_number);

        /**
         * @brief Sends Tune Request message.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_tune_request();

        /**
         * @brief Sends one system-common message.
         *
         * @param type System-common message type.
         * @param data1 Optional data byte.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_common(MessageType type, uint8_t data1 = 0);

        /**
         * @brief Sends one system real-time message.
         *
         * @param type System real-time message type.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_real_time(MessageType type);

        /**
         * @brief Sends MIDI Machine Control command as SysEx payload.
         *
         * @param device_id MMC target device identifier.
         * @param mmc MMC command type.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_mmc(uint8_t device_id, MessageType mmc);

        /**
         * @brief Sends NRPN sequence (7-bit or 14-bit value mode).
         *
         * @param parameter_number NRPN parameter number.
         * @param value Parameter value.
         * @param channel MIDI channel (1-16).
         * @param value_14bit When `true`, sends 14-bit value sequence.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send_nrpn(uint16_t parameter_number, uint16_t value, uint8_t channel, bool value_14bit = false);

        /**
         * @brief Sends a raw MIDI message.
         *
         * @param type MIDI message type.
         * @param data1 First data byte.
         * @param data2 Second data byte.
         * @param channel MIDI channel (1-16).
         *
         * @return `true` on success, otherwise `false`.
         */
        bool send(MessageType type, uint8_t data1, uint8_t data2, uint8_t channel);

        /**
         * @brief Reads and parses one MIDI message from transport.
         *
         * @return `true` when a complete message is decoded, otherwise `false`.
         */
        bool read();

        /**
         * @brief Parses pending bytes into one MIDI message.
         *
         * @return `true` when a complete message is decoded, otherwise `false`.
         */
        bool parse();

        /**
         * @brief Enables or disables recursive parsing mode.
         *
         * @param state New recursive parsing state.
         */
        void use_recursive_parsing(bool state);

        /**
         * @brief Returns current running-status mode.
         *
         * @return `true` when running status is enabled, otherwise `false`.
         */
        bool running_status_state();

        /**
         * @brief Enables or disables running-status mode for TX.
         *
         * @param state New running-status state.
         */
        void set_running_status_state(bool state);

        /**
         * @brief Returns active note-off mode.
         *
         * @return Active note-off mode.
         */
        NoteOffType note_off_mode();

        /**
         * @brief Sets note-off mode.
         *
         * @param type New note-off mode.
         */
        void set_note_off_mode(NoteOffType type);

        /**
         * @brief Returns last decoded message type.
         *
         * @return Last decoded message type.
         */
        MessageType type();

        /**
         * @brief Returns last decoded message channel.
         *
         * @return Last decoded message channel.
         */
        uint8_t channel();

        /**
         * @brief Returns first data byte of last decoded message.
         *
         * @return First data byte.
         */
        uint8_t data1();

        /**
         * @brief Returns second data byte of last decoded message.
         *
         * @return Second data byte.
         */
        uint8_t data2();

        /**
         * @brief Returns view of last decoded SysEx data array.
         *
         * @return Span over internal SysEx array.
         */
        std::span<uint8_t> sys_ex_array();

        /**
         * @brief Returns length of last decoded message.
         *
         * @return Message length in bytes.
         */
        uint16_t length();

        /**
         * @brief Registers one thru sink interface.
         *
         * @param interface_ref Thru interface to register.
         */
        void register_thru_interface(Thru& interface_ref);

        /**
         * @brief Unregisters one thru sink interface.
         *
         * @param interface_ref Thru interface to unregister.
         */
        void unregister_thru_interface(Thru& interface_ref);

        /**
         * @brief Returns reference to last decoded message object.
         *
         * @return Last decoded message object reference.
         */
        Message& message();

        private:
        Transport&                                                     _transport;
        Message                                                        _message                         = {};
        NoteOffType                                                    _note_off_mode                   = NoteOffType::NoteOnZeroVelocity;
        std::array<Thru*, CONFIG_ZLIBS_UTILS_MIDI_MAX_THRU_INTERFACES> _thru_interfaces                 = {};
        bool                                                           _initialized                     = false;
        bool                                                           _use_running_status              = false;
        bool                                                           _recursive_parse_state           = false;
        uint8_t                                                        _running_status_rx               = 0;
        uint8_t                                                        _running_status_tx               = 0;
        uint8_t                                                        _pending_message[3]              = {};
        uint16_t                                                       _pending_message_expected_length = 0;
        uint16_t                                                       _pending_message_index           = 0;
        mutable zlibs::utils::misc::Mutex                              _tx_mutex;

        /**
         * @brief Forwards the last decoded message to all registered thru interfaces.
         */
        void thru();

        /**
         * @brief Builds status byte for a message type/channel pair.
         *
         * @param type MIDI channel message type.
         * @param channel MIDI channel (1-16).
         *
         * @return Encoded MIDI status byte.
         */
        uint8_t status(MessageType type, uint8_t channel);
    };
}    // namespace zlibs::utils::midi
