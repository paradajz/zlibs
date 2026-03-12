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
     * @brief UART runtime configuration.
     */
    struct Config
    {
        static constexpr uint32_t DEFAULT_BAUDRATE = 115200;

        uint32_t                            baudrate  = DEFAULT_BAUDRATE;
        uart_config_stop_bits               stop_bits = UART_CFG_STOP_BITS_1;
        zlibs::utils::misc::RingBufferBase& rx_buffer;
        zlibs::utils::misc::RingBufferBase& tx_buffer;

        /**
         * @brief Creates a UART configuration object.
         *
         * @param baudrate UART baud rate in bits per second.
         * @param stop_bits UART stop-bit setting.
         * @param rx_buffer Receive buffer reference.
         * @param tx_buffer Transmit buffer reference.
         */
        Config(uint32_t                            baudrate,
               uart_config_stop_bits               stop_bits,
               zlibs::utils::misc::RingBufferBase& rx_buffer,
               zlibs::utils::misc::RingBufferBase& tx_buffer)
            : baudrate(baudrate)
            , stop_bits(stop_bits)
            , rx_buffer(rx_buffer)
            , tx_buffer(tx_buffer)
        {}
    };
}    // namespace zlibs::drivers::uart
