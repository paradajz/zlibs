/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/misc/version.h"

using namespace zlibs::utils;

namespace
{
    using TestVersion = misc::Version<1, 23, 456>;

    static_assert(TestVersion::MAJOR == 1);
    static_assert(TestVersion::MINOR == 23);
    static_assert(TestVersion::REVISION == 456);
    static_assert(std::string_view(TestVersion::STRING.c_str()) == "1.23.456");
}    // namespace
