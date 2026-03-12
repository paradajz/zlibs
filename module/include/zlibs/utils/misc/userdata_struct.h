/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>

#include <concepts>

namespace zlibs::utils::misc
{
    /**
     * @brief Helper wrapper that stores user data alongside a Zephyr C struct.
     *
     * @tparam S Embedded Zephyr struct type.
     * @tparam U User data type.
     */
    template<typename S, typename U>
    struct UserDataStruct
    {
        /** @brief Convenience alias for the current specialized wrapper type. */
        using Type = UserDataStruct<S, U>;

        S  member    = {};
        U* user_data = nullptr;

        /**
         * @brief Extracts user data from embedded member pointer.
         *
         * @param ptr Pointer to embedded member.
         *
         * @return Associated user data pointer.
         */
        static U* extract_user_data(void* ptr)
            requires(!std::same_as<S, k_work_delayable>)
        {
            auto user_data = CONTAINER_OF(static_cast<S*>(ptr), Type, member)->user_data;
            return user_data;
        }

        /**
         * @brief Extracts user data from delayable-work callback pointer.
         *
         * @param ptr Pointer passed to `k_work` callback.
         *
         * @return Associated user data pointer.
         */
        static U* extract_user_data(void* ptr)
            requires(std::same_as<S, k_work_delayable>)
        {
            // special case: for k_work_delayable, retrieve k_work first
            auto kwork     = k_work_delayable_from_work(static_cast<k_work*>(ptr));
            auto user_data = CONTAINER_OF(kwork, Type, member)->user_data;
            return user_data;
        }
    };
}    // namespace zlibs::utils::misc
