/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/threads/threads.h"

namespace zlibs::common::internal
{
    using Workqueue = zlibs::utils::threads::WorkqueueThread<zlibs::utils::misc::StringLiteral{ "zlibs" },
                                                             CONFIG_ZLIBS_INTERNAL_WORKQUEUE_PRIORITY,
                                                             CONFIG_ZLIBS_INTERNAL_WORKQUEUE_STACK_SIZE>;
}    // namespace zlibs::common::internal
