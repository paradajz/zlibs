/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/drivers/uart/uart_hw.h"

using namespace zlibs::drivers::uart;
using namespace ::testing;

namespace
{
    LOG_MODULE_REGISTER(uart_hw_test, CONFIG_ZLIBS_LOG_LEVEL);
}    // namespace

class UartHwTest : public ::testing::Test
{
    protected:
    const device* _device = NULL;
    UartHw        _uart   = UartHw(_device);

    virtual void SetUp() override
    {
        LOG_INF("Running test %s", UnitTest::GetInstance()->current_test_info()->name());
    }

    virtual void TearDown() override
    {
        LOG_INF("Test %s complete", UnitTest::GetInstance()->current_test_info()->name());
    }
};

TEST_F(UartHwTest, Dummy)
{}
