/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "usb_base.h"

#include "gmock/gmock.h"

namespace zlibs::drivers::usb
{
    /**
     * @brief GoogleMock-based USB mock implementation.
     */
    class UsbMock : public UsbBase
    {
        public:
        UsbMock()  = default;
        ~UsbMock() = default;

        MOCK_METHOD(bool, init, (const Config& config), (override));
        MOCK_METHOD(bool, deinit, (), (override));
    };
}    // namespace zlibs::drivers::usb
