/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/misc/noncopyable.h"

#include <type_traits>

using namespace zlibs::utils;

namespace
{
    struct DerivedNonCopyable : misc::NonCopyableOrMovable
    {
        int value = 42;
    };

    static_assert(std::is_default_constructible_v<DerivedNonCopyable>);
    static_assert(!std::is_copy_constructible_v<DerivedNonCopyable>);
    static_assert(!std::is_copy_assignable_v<DerivedNonCopyable>);
    static_assert(!std::is_move_constructible_v<DerivedNonCopyable>);
    static_assert(!std::is_move_assignable_v<DerivedNonCopyable>);
}    // namespace
