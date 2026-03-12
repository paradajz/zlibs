/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "uart_common.h"

namespace zlibs::drivers::uart
{
    /**
     * @brief Abstract UART device interface.
     */
    class UartBase
    {
        public:
        UartBase()          = default;
        virtual ~UartBase() = default;

        /**
         * @brief Initializes the UART instance.
         *
         * @param config UART driver configuration.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init(const Config& config) = 0;

        /**
         * @brief Deinitializes the UART instance.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool deinit() = 0;

        /**
         * @brief Writes a single byte.
         *
         * @param data Byte to send.
         *
         * @return `true` if queued/transmitted successfully, otherwise `false`.
         */
        virtual bool write(const uint8_t data) = 0;

        /**
         * @brief Reads a single byte.
         *
         * @return The received byte when available, otherwise `std::nullopt`.
         */
        virtual std::optional<uint8_t> read() = 0;
    };
}    // namespace zlibs::drivers::uart
