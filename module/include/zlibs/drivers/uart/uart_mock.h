/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "uart_base.h"

#include "gmock/gmock.h"

namespace zlibs::drivers::uart
{
    /**
     * @brief GoogleMock-based UART mock implementation.
     */
    class UartMock : public UartBase
    {
        public:
        UartMock()  = default;
        ~UartMock() = default;

        MOCK_METHOD(bool, init, (const Config& config), (override));
        MOCK_METHOD(bool, deinit, (), (override));
        MOCK_METHOD(bool, write, (const uint8_t data), (override));
        MOCK_METHOD(std::optional<uint8_t>, read, (), (override));
    };
}    // namespace zlibs::drivers::uart
