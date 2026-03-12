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
         * @brief Extracts user data from an opaque pointer to the embedded member.
         *
         * Available for wrappers whose embedded type is not `k_work_delayable`.
         *
         * @tparam P Pointer pointee type, either `void` or `const void`.
         * @param ptr `void*` or `const void*` pointing to the embedded member.
         *
         * @return Associated user data pointer.
         */
        template<typename P>
            requires(!std::same_as<S, k_work_delayable> && (std::same_as<P, void> || std::same_as<P, const void>))
        static U* extract_user_data(P* ptr)
        {
            using CastType = std::conditional_t<std::is_const_v<P>, const S*, S*>;
            return CONTAINER_OF(static_cast<CastType>(ptr), Type, member)->user_data;
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
