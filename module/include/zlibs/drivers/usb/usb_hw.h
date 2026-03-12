/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "common.h"
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
        ~UsbHw() override              = default;
        UsbHw(const UsbHw&)            = delete;
        UsbHw(UsbHw&&)                 = delete;
        UsbHw& operator=(const UsbHw&) = delete;
        UsbHw& operator=(UsbHw&&)      = delete;

        /**
         * @brief Returns the singleton USB hardware wrapper instance.
         *
         * The provided Zephyr USB device controller pointer may be updated
         * until the singleton is initialized. After successful initialization,
         * the active device binding is fixed for runtime use and cannot be
         * changed by subsequent `instance()` calls.
         *
         * @param device Zephyr USB device controller pointer.
         *
         * @return Singleton USB hardware wrapper instance.
         */
        static UsbHw& instance(const device* device);

        /**
         * @brief Initializes the USB device stack wrapper.
         *
         * @param config Optional USB initialization parameters.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init(const Config& config = {}) override;

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

        const device* _device      = nullptr;
        bool          _initialized = false;
    };
}    // namespace zlibs::drivers::usb
