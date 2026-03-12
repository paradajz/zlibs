/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "string.h"

#include <cinttypes>

namespace zlibs::utils::misc
{
    /**
     * @brief Compile-time semantic version descriptor.
     *
     * @tparam Major Major version number.
     * @tparam Minor Minor version number.
     * @tparam Revision Patch/revision number.
     */
    template<int Major, int Minor, int Revision>
    struct Version
    {
        static constexpr int  MAJOR    = Major;
        static constexpr int  MINOR    = Minor;
        static constexpr int  REVISION = Revision;
        static constexpr auto STRING   = zlibs::utils::misc::string_join(
            zlibs::utils::misc::int_to_string<MAJOR>(),
            zlibs::utils::misc::StringLiteral{ "." },
            zlibs::utils::misc::int_to_string<MINOR>(),
            zlibs::utils::misc::StringLiteral{ "." },
            zlibs::utils::misc::int_to_string<REVISION>());
    };
}    // namespace zlibs::utils::misc
