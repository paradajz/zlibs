/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "sysex_conf_deps.h"

namespace zlibs::utils::sysex_conf
{
    /**
     * @brief SysEx-based configuration protocol engine.
     *
     * The protocol operates over a user-provided layout and delegates data
     * reads, writes, custom requests, and frame transmission to `DataHandler`.
     */
    class SysExConf
    {
        public:
        /**
         * @brief Creates a protocol instance over a data handler and manufacturer ID.
         *
         * @param data_handler Backend implementation used for data access and
         *                     response transmission.
         * @param manufacturer_id Manufacturer identifier expected in incoming frames.
         */
        SysExConf(DataHandler& data_handler, const ManufacturerId& manufacturer_id)
            : _data_handler(data_handler)
            , _manufacturer_id(manufacturer_id)
        {}

        /**
         * @brief Clears protocol state, layout contents, and custom request table.
         */
        void reset();

        /**
         * @brief Sets the configuration layout handled by the protocol.
         *
         * Layout descriptors are intended to be created at compile time by
         * `make_block()` and `make_layout()`.
         *
         * @param layout Borrowed block layout.
         *
         * @return `true` when the layout is non-empty, otherwise `false`.
         */
        bool set_layout(std::span<const Block> layout);

        /**
         * @brief Registers user-defined special request handlers.
         *
         * @param custom_requests Custom request descriptors.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool setup_custom_requests(std::span<const CustomRequest> custom_requests);

        /**
         * @brief Processes one incoming SysEx message.
         *
         * @param message Encoded SysEx frame.
         */
        void handle_message(std::span<const uint8_t> message);

        /**
         * @brief Returns whether the configuration session is open.
         *
         * @return `true` when configuration session is enabled, otherwise `false`.
         */
        bool is_configuration_enabled();

        /**
         * @brief Enables or disables user-error ignore mode.
         *
         * @param state New ignore-mode state.
         */
        void set_user_error_ignore_mode(bool state);

        /**
         * @brief Sends a custom SysEx message assembled from raw values.
         *
         * SysEx start/end bytes and configured manufacturer-ID bytes are
         * emitted automatically.
         *
         * @param values Values to append after the common protocol header.
         * @param ack When `true`, emits `Status::Ack`; otherwise emits
         *            `Status::Request`.
         */
        void send_custom_message(std::span<const uint16_t> values, bool ack = true);

        /**
         * @brief Returns the number of configured blocks.
         *
         * @return Number of configured blocks.
         */
        uint8_t blocks() const;

        /**
         * @brief Returns the number of sections in one block.
         *
         * @param block_index Zero-based block index.
         *
         * @return Number of sections for the selected block.
         */
        uint8_t sections(uint8_t block_index) const;

        private:
        DataHandler&                   _data_handler;
        const ManufacturerId&          _manufacturer_id;
        uint8_t                        _response_array[MAX_MESSAGE_SIZE] = {};
        uint16_t                       _response_counter                 = 0;
        bool                           _sys_ex_enabled                   = false;
        bool                           _user_error_ignore_mode_enabled   = false;
        std::span<const Block>         _layout                           = {};
        std::span<const CustomRequest> _sys_ex_custom_request            = {};
        DecodedMessage                 _decoded_message                  = {};

        /**
         * @brief Appends one 14-bit value to the outgoing response payload.
         *
         * @param value Value to append.
         *
         * @return `true` if appended, otherwise `false` when response capacity is exceeded.
         */
        bool add_to_response(uint16_t value);

        /**
         * @brief Decodes and validates one received SysEx frame.
         *
         * @param message Incoming message bytes.
         *
         * @return `true` on successful decode/validation, otherwise `false`.
         */
        bool decode(std::span<const uint8_t> message);

        /**
         * @brief Resets decoded-message scratch fields to default state.
         */
        void reset_decoded_message();

        /**
         * @brief Handles standard GET/SET/BACKUP requests.
         *
         * @param received_array_size Number of bytes in the request frame.
         *
         * @return `true` if request processing completed successfully, otherwise `false`.
         */
        bool process_standard_request(uint16_t received_array_size);

        /**
         * @brief Handles built-in and user-defined special requests.
         *
         * @return `true` if request is recognized and handled, otherwise `false`.
         */
        bool process_special_request();

        /**
         * @brief Verifies manufacturer ID bytes in the active frame.
         *
         * @return `true` when ID matches configured manufacturer ID.
         */
        bool check_manufacturer_id();

        /**
         * @brief Verifies that request status byte is valid for an incoming request.
         *
         * @return `true` when status byte equals `Status::Request`.
         */
        bool check_status();

        /**
         * @brief Verifies decoded wish field.
         *
         * @return `true` when wish value is in supported range.
         */
        bool check_wish();

        /**
         * @brief Verifies decoded amount field.
         *
         * @return `true` when amount value is in supported range.
         */
        bool check_amount();

        /**
         * @brief Verifies decoded block index against active layout.
         *
         * @return `true` when block index exists.
         */
        bool check_block();

        /**
         * @brief Verifies decoded section index against decoded block.
         *
         * @return `true` when section index exists.
         */
        bool check_section();

        /**
         * @brief Verifies decoded part field for the active request mode.
         *
         * @return `true` when part selector/index is valid.
         */
        bool check_part();

        /**
         * @brief Verifies decoded parameter index for the active section.
         *
         * @return `true` when index is in range.
         */
        bool check_parameter_index();

        /**
         * @brief Verifies decoded new value against section constraints.
         *
         * @return `true` when value is allowed.
         */
        bool check_new_value();

        /**
         * @brief Placeholder for legacy parameter checks.
         *
         * @return Validation result.
         */
        bool check_parameters();

        /**
         * @brief Computes expected message length from decoded fields.
         *
         * @return Expected frame length in bytes.
         */
        uint16_t generate_message_length();

        /**
         * @brief Writes protocol status byte into the active response frame.
         *
         * @tparam T Enum/integer type convertible to `uint8_t`.
         * @param status Status value to encode.
         */
        template<typename T>
        void set_status(T status)
        {
            uint8_t status_uint8 = static_cast<uint8_t>(status);
            status_uint8 &= MIDI_DATA_MASK;

            _response_array[static_cast<uint8_t>(ByteOrder::StatusByte)] = status_uint8;
        }

        /**
         * @brief Sends currently assembled response frame through data handler.
         *
         * @param contains_last_byte When `false`, appends `SYSEX_END_BYTE`
         *                           before dispatching the frame.
         */
        void send_response(bool contains_last_byte);
    };
}    // namespace zlibs::utils::sysex_conf
