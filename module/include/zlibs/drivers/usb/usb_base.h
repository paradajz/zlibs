/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "common.h"

namespace zlibs::drivers::usb
{
    /**
     * @brief Abstract USB device interface.
     */
    class UsbBase
    {
        public:
        UsbBase()          = default;
        virtual ~UsbBase() = default;

        /**
         * @brief Initializes the USB instance.
         *
         * @param config Optional USB initialization parameters.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool init(const Config& config = {}) = 0;

        /**
         * @brief Deinitializes the USB instance.
         *
         * @return `true` on success, otherwise `false`.
         */
        virtual bool deinit() = 0;
    };
}    // namespace zlibs::drivers::usb
