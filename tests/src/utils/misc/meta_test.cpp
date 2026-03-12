/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/misc/meta.h"

using namespace zlibs::utils;

namespace
{
    template<typename T>
    struct IsConstPointer : std::false_type
    {
    };

    template<typename T>
    struct IsConstPointer<const T*> : std::true_type
    {
    };

    static_assert(misc::UniqueInPack<int, int, float, bool>);
    static_assert(!misc::UniqueInPack<int, int, float, int>);
    static_assert(!misc::UniqueInPack<int, float, bool>);

    static_assert(misc::AllUnique<int, float, bool, double>);
    static_assert(!misc::AllUnique<int, float, int, double>);

    static_assert(misc::PackContains<float, int, float, bool>);
    static_assert(!misc::PackContains<long, int, float, bool>);

    static_assert(std::is_same_v<typename misc::FindFirst<std::is_integral, float, bool, int, long>::type, bool>);
    static_assert(std::is_same_v<typename misc::FindFirst<IsConstPointer, float, int*, const char*, const int*>::type, const char*>);
    static_assert(std::is_same_v<typename misc::FindFirst<std::is_void, int, float, bool>::type, void>);
}    // namespace
