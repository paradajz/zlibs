/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <span>

#include <zephyr/usb/usbd.h>

namespace zlibs::drivers::usb
{
    /**
     * @brief Optional USB initialization parameters applied before the stack is started.
     */
    struct Config
    {
        /**
         * @brief Optional Zephyr USB device message callback.
         */
        usbd_msg_cb_t message_callback = nullptr;

        /**
         * @brief Extra descriptor nodes to register before stack initialization.
         */
        std::span<usbd_desc_node* const> extra_descriptors = {};
    };
}    // namespace zlibs::drivers::usb
