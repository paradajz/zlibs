/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "uart_base.h"
#include "zlibs/utils/misc/mutex.h"

namespace zlibs::drivers::uart
{
    /**
     * @brief Hardware-backed UART implementation using a Zephyr UART device.
     */
    class UartHw : public UartBase
    {
        public:
        /**
         * @brief Creates a UART hardware wrapper.
         *
         * @param device Zephyr UART device pointer.
         */
        explicit UartHw(const device* device);

        ~UartHw() override;

        bool                   init(const Config& config) override;
        bool                   deinit() override;
        bool                   write(const uint8_t data) override;
        std::optional<uint8_t> read() override;

        private:
        const device*             _device      = nullptr;
        bool                      _initialized = false;
        bool                      _loopback    = false;
        RingBufferBase*           _rx_buffer   = nullptr;
        RingBufferBase*           _tx_buffer   = nullptr;
        zlibs::utils::misc::Mutex _mutex;

        /**
         * @brief Applies low-level UART hardware configuration.
         *
         * @param config Requested UART configuration.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool configure(const Config& config);

        /**
         * @brief Releases UART runtime resources without relying on virtual dispatch.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool shutdown();

        /**
         * @brief Interrupt service routine callback for Zephyr UART driver.
         *
         */
        void isr();
    };
}    // namespace zlibs::drivers::uart
