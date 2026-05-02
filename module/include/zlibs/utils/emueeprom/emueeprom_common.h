/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace zlibs::utils::emueeprom
{
    /**
     * @brief Logical value stored in erased entries.
     */
    constexpr inline uint16_t ERASED_VALUE = 0xFFFF;

    /**
     * @brief Byte value returned by erased flash.
     */
    constexpr inline uint8_t ERASED_BYTE = static_cast<uint8_t>(ERASED_VALUE);

    /**
     * @brief One logical EEPROM record.
     */
    struct Entry
    {
        uint16_t address = 0;
        uint16_t value   = 0;
    };

    /**
     * @brief Logical entry address reserved for internal page-status records.
     */
    constexpr inline uint16_t RESERVED_STATUS_ADDRESS = ERASED_VALUE;

    /**
     * @brief Byte offset of the value field inside a serialized EEPROM entry.
     */
    constexpr inline size_t SERIALIZED_VALUE_OFFSET = 0;

    /**
     * @brief Byte offset of the address field inside a serialized EEPROM entry.
     */
    constexpr inline size_t SERIALIZED_ADDRESS_OFFSET = sizeof(Entry::value);

    /**
     * @brief Size of one serialized EEPROM entry in bytes.
     */
    constexpr inline size_t SERIALIZED_ENTRY_SIZE = sizeof(Entry::value) + sizeof(Entry::address);

    /**
     * @brief Internal block size used for bulk reads and buffered writes.
     */
    constexpr inline size_t INTERNAL_BLOCK_SIZE = 256;

    /**
     * @brief Number of backend write blocks reserved for internal records.
     */
    constexpr inline size_t MIN_INTERNAL_ENTRY_BLOCK_COUNT = 3;

    /**
     * @brief Calculates how many logical addresses can fit in one page.
     *
     * The result is also the exclusive upper bound for valid addresses: if this
     * returns `N`, callers may use addresses `0` through `N - 1`.
     *
     * @param page_size EmuEEPROM page size in bytes.
     * @param write_block_size Backend write-block size in bytes.
     *
     * @return Logical address count available to users.
     */
    constexpr size_t address_count_for(size_t page_size, size_t write_block_size)
    {
        const size_t page_entry_count          = page_size / SERIALIZED_ENTRY_SIZE;
        const size_t entries_per_backend_block = write_block_size / SERIALIZED_ENTRY_SIZE;
        const size_t internal_entry_count      = MIN_INTERNAL_ENTRY_BLOCK_COUNT * entries_per_backend_block;

        if (page_entry_count <= internal_entry_count)
        {
            return 0;
        }

        const size_t address_count = page_entry_count - internal_entry_count;

        return address_count > RESERVED_STATUS_ADDRESS ? RESERVED_STATUS_ADDRESS : address_count;
    }

    /**
     * @brief Creates one logical EEPROM entry.
     *
     * @param address Logical EEPROM address.
     * @param value 16-bit value stored at the address.
     *
     * @return Logical EEPROM entry.
     */
    constexpr Entry make_entry(uint16_t address, uint16_t value)
    {
        return Entry{
            .address = address,
            .value   = value,
        };
    }

    /**
     * @brief Flash-page state encoded in reserved page-status entries.
     */
    enum class PageStatus : uint16_t
    {
        Valid     = 0x0000,
        Erased    = ERASED_VALUE,
        Formatted = 0xEEEE,
        Receiving = 0x7777,
    };

    /**
     * @brief Result of a logical EEPROM read.
     */
    enum class ReadStatus : uint8_t
    {
        Ok,
        NoVariable,
        NoPage,
        ReadError,
    };

    /**
     * @brief Result of a logical EEPROM write or page transfer.
     */
    enum class WriteStatus : uint8_t
    {
        Ok,
        PageFull,
        NoPage,
        WriteError,
    };

    /**
     * @brief Flash page roles used by the emulation algorithm.
     */
    enum class Page : uint8_t
    {
        Page1,
        Page2,
        Factory,
    };

    /**
     * @brief Writes an unsigned integer to a byte span in little-endian order.
     *
     * @tparam Value Unsigned integer type to serialize.
     *
     * @param value Value to serialize.
     * @param data Output byte span. Must be at least `sizeof(Value)` bytes.
     */
    template<typename Value>
    void serialize_value(Value value, std::span<uint8_t> data)
    {
        for (size_t index = 0; index < sizeof(Value); index++)
        {
            data[index] = static_cast<uint8_t>((value >> (index * std::numeric_limits<uint8_t>::digits)) & ERASED_BYTE);
        }
    }

    /**
     * @brief Reads an unsigned integer from a byte span in little-endian order.
     *
     * @tparam Value Unsigned integer type to deserialize.
     *
     * @param data Input byte span. Must be at least `sizeof(Value)` bytes.
     *
     * @return Deserialized value.
     */
    template<typename Value>
    Value deserialize_value(std::span<const uint8_t> data)
    {
        Value value = 0;

        for (size_t index = 0; index < sizeof(Value); index++)
        {
            value |= static_cast<Value>(data[index]) << (index * std::numeric_limits<uint8_t>::digits);
        }

        return value;
    }

    /**
     * @brief Serializes one logical entry into the shared EEPROM entry byte format.
     *
     * @param entry Logical EEPROM entry to serialize.
     * @param data Output byte span. Must be at least `SERIALIZED_ENTRY_SIZE` bytes.
     */
    inline void serialize_entry(Entry entry, std::span<uint8_t> data)
    {
        serialize_value(entry.value, data.subspan(SERIALIZED_VALUE_OFFSET, sizeof(Entry::value)));
        serialize_value(entry.address, data.subspan(SERIALIZED_ADDRESS_OFFSET, sizeof(Entry::address)));
    }

    /**
     * @brief Decodes one serialized EEPROM entry into its logical representation.
     *
     * @param data Serialized flash entry bytes.
     *
     * @return Logical entry.
     */
    inline Entry deserialize_entry(std::span<const uint8_t> data)
    {
        return make_entry(deserialize_value<decltype(Entry::address)>(data.subspan(SERIALIZED_ADDRESS_OFFSET, sizeof(Entry::address))),
                          deserialize_value<decltype(Entry::value)>(data.subspan(SERIALIZED_VALUE_OFFSET, sizeof(Entry::value))));
    }
}    // namespace zlibs::utils::emueeprom
