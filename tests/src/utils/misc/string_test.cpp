/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/misc/string.h"

using namespace ::testing;
using namespace zlibs::utils;

namespace
{
    LOG_MODULE_REGISTER(misc_string_test, CONFIG_ZLIBS_LOG_LEVEL);

    constexpr auto JOINED_RAW_LITERALS    = misc::string_join("zlibs", "/", "misc");
    constexpr auto JOINED_STRING_LITERALS = misc::string_join(misc::StringLiteral{ "native" }, misc::StringLiteral{ "-" }, misc::StringLiteral{ "sim" });

    static_assert(static_cast<std::string_view>(JOINED_RAW_LITERALS) == "zlibs/misc");
    static_assert(std::string_view(JOINED_STRING_LITERALS.c_str()) == "native-sim");
}    // namespace

class StringTest : public Test
{
    public:
    StringTest()  = default;
    ~StringTest() = default;

    protected:
    void SetUp() override
    {
        LOG_INF("Running test %s", UnitTest::GetInstance()->current_test_info()->name());
    }

    void TearDown() override
    {
        LOG_INF("Test %s complete", UnitTest::GetInstance()->current_test_info()->name());
    }
};

TEST_F(StringTest, StringLiteralExposesViewAndCString)
{
    constexpr misc::StringLiteral LITERAL{ "test-name" };

    ASSERT_EQ("test-name", static_cast<std::string_view>(LITERAL));
    ASSERT_STREQ("test-name", LITERAL.c_str());
}

TEST_F(StringTest, StringJoinBuildsExpectedString)
{
    constexpr auto JOINED = misc::string_join("module", "::", "string");

    ASSERT_EQ("module::string", static_cast<std::string_view>(JOINED));
    ASSERT_STREQ("module::string", JOINED.c_str());
}
