/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "noncopyable.h"

#include <zephyr/kernel.h>

#include <mutex>

namespace zlibs::utils::misc
{
    /**
     * @brief Thin BasicLockable wrapper for a Zephyr mutex.
     */
    class Mutex : public NonCopyableOrMovable
    {
        public:
        /**
         * @brief Creates and initializes mutex.
         */
        explicit Mutex()
        {
            k_mutex_init(&_mutex);
        }

        ~Mutex() = default;

        /**
         * @brief Locks mutex indefinitely.
         */
        void lock()
        {
            k_mutex_lock(&_mutex, K_FOREVER);
        }

        /**
         * @brief Unlocks mutex.
         */
        void unlock()
        {
            k_mutex_unlock(&_mutex);
        }

        private:
        k_mutex _mutex = {};
    };

    /**
     * @brief RAII guard that locks a Mutex on construction and unlocks it on destruction.
     */
    using LockGuard = std::lock_guard<Mutex>;
}    // namespace zlibs::utils::misc
