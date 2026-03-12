/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/filters/filters.h"
#include "zlibs/common/threads.h"

using namespace zlibs::utils::filters;

Debounce::Debounce(uint32_t debounce_time_ms)
    : _debounce_time_ms(debounce_time_ms)
    , _kwork_update([this]()
                    {
                        _stable_state = _new_state;
                    },
                    []()
                    {
                        return zlibs::common::internal::Workqueue::handle();
                    })
{
}
