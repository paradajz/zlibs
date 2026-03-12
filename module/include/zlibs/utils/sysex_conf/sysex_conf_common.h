/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/midi/midi_common.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace zlibs::utils::sysex_conf
{
    /**
     * @brief Supported request operations encoded in the SysEx wish byte.
     */
    enum class Wish : uint8_t
    {
        Get,
        Set,
        Backup,
        Invalid
    };

    /**
     * @brief Supported request scopes encoded in the SysEx amount byte.
     */
    enum class Amount : uint8_t
    {
        Single,
        All,
        Invalid
    };

    /**
     * @brief Protocol status codes returned in SysEx responses.
     */
    enum class Status : uint8_t
    {
        Request,               // 0x00
        Ack,                   // 0x01
        ErrorStatus,           // 0x02
        ErrorConnection,       // 0x03
        ErrorWish,             // 0x04
        ErrorAmount,           // 0x05
        ErrorBlock,            // 0x06
        ErrorSection,          // 0x07
        ErrorPart,             // 0x08
        ErrorIndex,            // 0x09
        ErrorNewValue,         // 0x0A
        ErrorMessageLength,    // 0x0B
        ErrorWrite,            // 0x0C
        ErrorNotSupported,     // 0x0D
        ErrorRead,             // 0x0E
    };

    /**
     * @brief Reserved special request identifiers.
     */
    enum class SpecialRequest : uint8_t
    {
        ConnClose,           // 0x00
        ConnOpen,            // 0x01
        BytesPerValue,       // 0x02
        ParamsPerMessage,    // 0x03
        Count
    };

    /**
     * @brief Byte offsets used by the SysEx protocol frame.
     */
    enum class ByteOrder : uint8_t
    {
        StartByte,      // 0
        IdByte1,        // 1
        IdByte2,        // 2
        IdByte3,        // 3
        StatusByte,     // 4
        PartByte,       // 5
        WishByte,       // 6
        AmountByte,     // 7
        BlockByte,      // 8
        SectionByte,    // 9
        IndexByte,      // 10
    };

    /**
     * @brief Three-byte SysEx manufacturer identifier.
     */
    struct ManufacturerId
    {
        uint8_t id1 = 0;
        uint8_t id2 = 0;
        uint8_t id3 = 0;
    };

    /**
     * @brief Metadata describing one user-defined special request.
     */
    struct CustomRequest
    {
        uint16_t request_id      = 0;        ///< Request identifier carried in the wish byte.
        bool     conn_open_check = false;    ///< When `true`, request is accepted only while the connection is open.
    };

    /**
     * @brief Decoded fields extracted from one incoming SysExConf request.
     */
    struct DecodedRequest
    {
        Status   status    = {};
        Wish     wish      = {};
        Amount   amount    = {};
        uint8_t  block     = 0;
        uint8_t  section   = 0;
        uint8_t  part      = 0;
        uint16_t index     = 0;
        uint16_t new_value = 0;
    };

    /** @brief SysEx frame start byte (`0xF0`). */
    constexpr inline uint8_t SYSEX_START_BYTE = midi::SYS_EX_START;

    /** @brief SysEx frame end byte (`0xF7`). */
    constexpr inline uint8_t SYSEX_END_BYTE = midi::SYS_EX_END;

    /** @brief Special part selector requesting all parts (`0x7F`). */
    constexpr inline uint8_t PART_ALL_MESSAGES = 127;

    /** @brief Special part selector requesting all parts plus trailing ACK (`0x7E`). */
    constexpr inline uint8_t PART_ALL_MESSAGES_WITH_ACK = 126;

    /** @brief Part value used in the final ACK frame for all-parts transfers (`0x7E`). */
    constexpr inline uint8_t PART_ALL_MESSAGES_ACK_PART = 0x7E;

    /** @brief Maximum number of parameter values carried per message part. */
    constexpr inline uint16_t PARAMS_PER_MESSAGE = 32;

    /** @brief Number of transport bytes used to encode one parameter value. */
    constexpr inline uint16_t BYTES_PER_VALUE = 2;

    /** @brief MIDI data-byte mask (`0x7F`). */
    constexpr inline uint8_t MIDI_DATA_MASK = midi::MIDI_DATA_BYTE_MASK;

    /** @brief Mask for one 14-bit value payload (`0x3FFF`). */
    constexpr inline uint16_t VALUE_14BIT_MASK = 0x3FFF;

    /** @brief Size of a special request frame including terminator byte. */
    constexpr inline uint8_t SPECIAL_REQ_MSG_SIZE = (static_cast<uint8_t>(ByteOrder::WishByte) + 1) + 1;

    /** @brief Minimum size of a standard request frame. */
    constexpr inline uint8_t STD_REQ_MIN_MSG_SIZE = static_cast<uint8_t>(ByteOrder::IndexByte) + (BYTES_PER_VALUE * 2) + 1;

    /** @brief Maximum supported SysEx message size handled by this module. */
    constexpr inline uint16_t MAX_MESSAGE_SIZE = STD_REQ_MIN_MSG_SIZE + (PARAMS_PER_MESSAGE * BYTES_PER_VALUE);

    /**
     * @brief Section descriptor defining parameter count and accepted value range.
     *
     * This descriptor is immutable and intended for compile-time construction.
     */
    class Section
    {
        public:
        /**
         * @brief Constructs a section descriptor.
         *
         * @param number_of_parameters Number of parameters in this section.
         * @param new_value_min Minimum allowed value for SET requests.
         * @param new_value_max Maximum allowed value for SET requests.
         */
        consteval Section(uint16_t number_of_parameters, uint16_t new_value_min, uint16_t new_value_max)
            : _number_of_parameters_value(number_of_parameters)
            , _new_value_min_value(new_value_min)
            , _new_value_max_value(new_value_max)
            , _parts_value(
                  static_cast<uint8_t>((number_of_parameters / PARAMS_PER_MESSAGE) +
                                       ((number_of_parameters % PARAMS_PER_MESSAGE) ? 1 : 0)))
        {}

        /**
         * @brief Returns the number of parameters stored in this section.
         */
        uint16_t number_of_parameters() const
        {
            return _number_of_parameters_value;
        }

        /**
         * @brief Returns the minimum accepted value for SET requests.
         */
        uint16_t new_value_min() const
        {
            return _new_value_min_value;
        }

        /**
         * @brief Returns the maximum accepted value for SET requests.
         */
        uint16_t new_value_max() const
        {
            return _new_value_max_value;
        }

        /**
         * @brief Returns the number of transport parts needed for this section.
         */
        uint8_t parts() const
        {
            return _parts_value;
        }

        private:
        const uint16_t _number_of_parameters_value;
        const uint16_t _new_value_min_value;
        const uint16_t _new_value_max_value;
        const uint8_t  _parts_value;
    };

    /**
     * @brief Borrowed group of sections forming one logical SysEx block.
     *
     * This descriptor is immutable and intended for compile-time construction.
     */
    class Block
    {
        public:
        /**
         * @brief Constructs a block over borrowed section metadata.
         *
         * @param sections Section descriptors that must outlive this block.
         */
        template<size_t N>
        consteval explicit Block(const std::array<Section, N>& sections)
            : _sections(sections)
        {}

        private:
        friend class SysExConf;
        std::span<const Section> _sections;
    };

    /**
     * @brief Builds a SysEx section array as a compile-time layout unit.
     *
     * This helper mirrors lessdb's API style and enforces compile-time
     * construction through `consteval`.
     *
     * @param sections Compile-time section array.
     *
     * @return The same section array.
     */
    template<size_t N>
    consteval std::array<Section, N> make_block(const std::array<Section, N>& sections)
    {
        return sections;
    }

    /**
     * @brief Builds a SysEx block array as a compile-time layout descriptor.
     *
     * This helper mirrors lessdb's API style and enforces compile-time
     * construction through `consteval`.
     *
     * @param blocks Compile-time block array.
     *
     * @return The same block array.
     */
    template<size_t N>
    consteval std::array<Block, N> make_layout(const std::array<Block, N>& blocks)
    {
        return blocks;
    }

    /** @brief Merges two 7-bit SysEx bytes into one 14-bit value. */
    using Merge14Bit = midi::Merge14Bit;

    /** @brief Splits one 14-bit value into two 7-bit SysEx bytes. */
    using Split14Bit = midi::Split14Bit;
}    // namespace zlibs::utils::sysex_conf
