/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/misc/version.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

namespace
{
    LOG_MODULE_REGISTER(main);

    constexpr uint32_t SLEEP_TIME_MS = 100;
    constexpr auto     VERSION       = zlibs::utils::misc::Version<1, 0, 0>();
}    // namespace

int main()
{
    LOG_INF("Software version is %s", VERSION.STRING.c_str());

    while (1)
    {
        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
}
