/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/misc/ring_buffer.h"

#include <zephyr/drivers/uart.h>

namespace zlibs::drivers::uart
{
    /**
     * @brief Abstract interface for ring buffer.
     */
    class RingBufferBase
    {
        public:
        RingBufferBase()          = default;
        virtual ~RingBufferBase() = default;

        /**
         * @brief Gets the number of elements currently stored.
         *
         * @return Number of stored bytes.
         */
        virtual size_t size() = 0;

        /**
         * @brief Checks whether the buffer is empty.
         *
         * @return `true` if empty, otherwise `false`.
         */
        virtual bool is_empty() = 0;

        /**
         * @brief Checks whether the buffer is full.
         *
         * @return `true` if full, otherwise `false`.
         */
        virtual bool is_full() = 0;

        /**
         * @brief Inserts one byte into the buffer.
         *
         * @param data Byte to insert.
         *
         * @return `true` on success, `false` if buffer is full.
         */
        virtual bool insert(uint8_t data) = 0;

        /**
         * @brief Reads the next element without removing it.
         *
         * @return The next element when available, otherwise std::nullopt.
         */
        virtual std::optional<uint8_t> peek() = 0;

        /**
         * @brief Removes and returns the next byte.
         *
         * @return The removed byte when available, otherwise `std::nullopt`.
         */
        virtual std::optional<uint8_t> remove() = 0;

        /**
         * @brief Clears all buffered data.
         */
        virtual void reset() = 0;
    };

    template<size_t MaxSize, bool OverwriteOldest = false>
    class RingBuffer : public RingBufferBase, public zlibs::utils::misc::RingBuffer<MaxSize, OverwriteOldest, uint8_t>
    {
        public:
        size_t size() override
        {
            return zlibs::utils::misc::RingBuffer<MaxSize, OverwriteOldest, uint8_t>::size();
        }

        bool is_empty() override
        {
            return zlibs::utils::misc::RingBuffer<MaxSize, OverwriteOldest, uint8_t>::is_empty();
        }

        bool is_full() override
        {
            return zlibs::utils::misc::RingBuffer<MaxSize, OverwriteOldest, uint8_t>::is_full();
        }

        bool insert(uint8_t data) override
        {
            return zlibs::utils::misc::RingBuffer<MaxSize, OverwriteOldest, uint8_t>::insert(data);
        }

        std::optional<uint8_t> peek() override
        {
            return zlibs::utils::misc::RingBuffer<MaxSize, OverwriteOldest, uint8_t>::peek();
        }

        std::optional<uint8_t> remove() override
        {
            return zlibs::utils::misc::RingBuffer<MaxSize, OverwriteOldest, uint8_t>::remove();
        }

        void reset() override
        {
            zlibs::utils::misc::RingBuffer<MaxSize, OverwriteOldest, uint8_t>::reset();
        }
    };

    /**
     * @brief UART runtime configuration.
     */
    struct Config
    {
        static constexpr uint32_t DEFAULT_BAUDRATE = 115200;

        uint32_t              baudrate  = DEFAULT_BAUDRATE;
        uart_config_stop_bits stop_bits = UART_CFG_STOP_BITS_1;
        RingBufferBase&       rx_buffer;
        RingBufferBase&       tx_buffer;
        bool                  loopback = false;

        /**
         * @brief Creates a UART configuration object.
         *
         * @param baudrate UART baud rate in bits per second.
         * @param stop_bits UART stop-bit setting.
         * @param rx_buffer Receive buffer reference.
         * @param tx_buffer Transmit buffer reference.
         * @param loopback When enabled, every received byte is echoed back to TX.
         */
        Config(uint32_t              baudrate,
               uart_config_stop_bits stop_bits,
               RingBufferBase&       rx_buffer,
               RingBufferBase&       tx_buffer,
               bool                  loopback = false)
            : baudrate(baudrate)
            , stop_bits(stop_bits)
            , rx_buffer(rx_buffer)
            , tx_buffer(tx_buffer)
            , loopback(loopback)
        {}
    };
}    // namespace zlibs::drivers::uart
