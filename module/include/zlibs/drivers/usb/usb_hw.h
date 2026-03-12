/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "usb_base.h"

#include <zephyr/device.h>

namespace zlibs::drivers::usb
{
    /**
     * @brief Hardware-backed USB implementation using a Zephyr USB device controller.
     *
     * This wrapper is implemented as a singleton because it manages a single
     * global USB device stack context.
     */
    class UsbHw : public UsbBase
    {
        public:
        /**
         * @brief Returns the singleton USB hardware wrapper instance.
         *
         * The first call binds the singleton to the provided Zephyr USB device
         * controller pointer. Subsequent calls reuse the same instance.
         *
         * @param device Zephyr USB device controller pointer.
         *
         * @return Singleton USB hardware wrapper instance.
         */
        static UsbHw& instance(const device* device);

        ~UsbHw() override = default;

        UsbHw(const UsbHw&)            = delete;
        UsbHw(UsbHw&&)                 = delete;
        UsbHw& operator=(const UsbHw&) = delete;
        UsbHw& operator=(UsbHw&&)      = delete;

        /**
         * @brief Initializes the USB device stack wrapper.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init() override;

        /**
         * @brief Deinitializes the USB device stack wrapper.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool deinit() override;

        private:
        /**
         * @brief Creates a USB hardware wrapper singleton instance.
         *
         * @param device Zephyr USB device controller pointer.
         */
        explicit UsbHw(const device* device);

        const device* _device = nullptr;
    };
}    // namespace zlibs::drivers::usb
