/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace zlibs::utils::misc
{
    /**
     * @brief Utility base type that disables copy and move semantics.
     */
    struct NonCopyableOrMovable
    {
        NonCopyableOrMovable()  = default;
        ~NonCopyableOrMovable() = default;

        NonCopyableOrMovable(const NonCopyableOrMovable&)            = delete;
        NonCopyableOrMovable& operator=(const NonCopyableOrMovable&) = delete;
        NonCopyableOrMovable(NonCopyableOrMovable&&)                 = delete;
        NonCopyableOrMovable& operator=(NonCopyableOrMovable&&)      = delete;
    };
}    // namespace zlibs::utils::misc
