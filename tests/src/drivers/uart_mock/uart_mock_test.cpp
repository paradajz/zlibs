/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/drivers/uart/uart_mock.h"

using namespace zlibs::drivers::uart;
using namespace ::testing;

namespace
{
    LOG_MODULE_REGISTER(uart_mock_test, CONFIG_ZLIBS_LOG_LEVEL);
}    // namespace

class UartMockTest : public ::testing::Test
{
    protected:
    UartMock _uart;

    virtual void SetUp() override
    {
        LOG_INF("Running test %s", UnitTest::GetInstance()->current_test_info()->name());
    }

    virtual void TearDown() override
    {
        LOG_INF("Test %s complete", UnitTest::GetInstance()->current_test_info()->name());
    }
};

TEST_F(UartMockTest, Dummy)
{}
