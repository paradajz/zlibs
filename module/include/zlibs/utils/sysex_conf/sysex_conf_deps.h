/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "sysex_conf_common.h"

namespace zlibs::utils::sysex_conf
{
    class SysExConf;

    /**
     * @brief Hardware and storage callback interface used by `SysExConf`.
     */
    class DataHandler
    {
        public:
        /**
         * @brief Response builder passed to custom request handlers.
         *
         * Custom handlers append 14-bit values and the protocol layer keeps
         * the framing bytes and final terminator management.
         */
        class CustomResponse
        {
            public:
            /**
             * @brief Appends one 14-bit encoded value to the active response.
             *
             * Capacity is validated internally.
             *
             * @param value Value to append. Only the lower 14 bits are used.
             *
             * @return `true` when value is appended, otherwise `false` when
             * response capacity is exhausted.
             */
            bool append(uint16_t value)
            {
                value &= VALUE_14BIT_MASK;

                // Keep one byte reserved for the final SysEx terminator.
                if ((static_cast<size_t>(_response_counter) + BYTES_PER_VALUE) >= _response_buffer.size())
                {
                    _overflowed = true;
                    return false;
                }

                auto split                            = Split14Bit(value);
                _response_buffer[_response_counter++] = split.high();
                _response_buffer[_response_counter++] = split.low();

                return true;
            }

            private:
            friend class SysExConf;

            /**
             * @brief Creates a response-appending helper for custom requests.
             *
             * @param response_buffer Borrowed response frame buffer.
             * @param response_counter Reference to active response length counter.
             */
            CustomResponse(std::span<uint8_t> response_buffer, uint16_t& response_counter)
                : _response_buffer(response_buffer)
                , _response_counter(response_counter)
            {
            }

            std::span<uint8_t> _response_buffer;
            uint16_t&          _response_counter;
            bool               _overflowed = false;
        };

        DataHandler()          = default;
        virtual ~DataHandler() = default;

        /**
         * @brief Reads one value from the user data store.
         *
         * @param block Block index.
         * @param section Section index within @p block.
         * @param index Parameter index within @p section.
         * @param value Output reference that receives the read value.
         *
         * @return `Status` result code.
         */
        virtual Status get(uint8_t block, uint8_t section, uint16_t index, uint16_t& value) = 0;

        /**
         * @brief Writes one value to the user data store.
         *
         * @param block Block index.
         * @param section Section index within @p block.
         * @param index Parameter index within @p section.
         * @param new_value New value to write.
         *
         * @return `Status` result code.
         */
        virtual Status set(uint8_t block, uint8_t section, uint16_t index, uint16_t new_value) = 0;

        /**
         * @brief Processes one user-defined custom request.
         *
         * @param request Custom request ID.
         * @param custom_response Response helper used to append payload values.
         *
         * If any append operation overflows response capacity, SysExConf reports
         * `Status::ErrorRead` regardless of the returned status value.
         *
         * @return `Status` result code.
         */
        virtual Status custom_request(uint16_t request, CustomResponse& custom_response) = 0;

        /**
         * @brief Sends a fully encoded SysEx response frame.
         *
         * @param response Encoded response bytes.
         */
        virtual void send_response(std::span<const uint8_t> response) = 0;
    };
}    // namespace zlibs::utils::sysex_conf
