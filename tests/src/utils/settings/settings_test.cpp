/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/settings/settings.h"

#include <zephyr/settings/settings.h>

#include <array>
#include <cstring>

using namespace zlibs::utils;
using namespace ::testing;

namespace
{
    LOG_MODULE_REGISTER(settings_test, CONFIG_ZLIBS_LOG_LEVEL);

    struct ScalarSettingKeepOnReset
    {
        struct Value
        {
            uint8_t  channel;
            uint32_t frequency;
            bool     enabled;

            bool operator==(const Value&) const = default;
        };

        struct Properties
        {
            static constexpr auto  KEY           = misc::StringLiteral{ "ScalarSettingKeepOnReset" };
            static constexpr Value DEFAULT_VALUE = { .channel = 1, .frequency = 868000000, .enabled = true };
            static constexpr bool  KEEP_ON_RESET = true;

            static constexpr bool validate(const Value& v)
            {
                return v.channel >= 1 && v.channel <= 16;
            }
        };
    };

    struct ScalarSettingNoKeepOnReset
    {
        struct Value
        {
            uint8_t  channel;
            uint32_t frequency;
            bool     enabled;

            bool operator==(const Value&) const = default;
        };

        struct Properties
        {
            static constexpr auto  KEY           = misc::StringLiteral{ "ScalarSettingNoKeepOnReset" };
            static constexpr Value DEFAULT_VALUE = { .channel = 1, .frequency = 868000000, .enabled = true };
            static constexpr bool  KEEP_ON_RESET = false;

            static constexpr bool validate(const Value& v)
            {
                return v.channel >= 1 && v.channel <= 16;
            }
        };
    };

    struct StringSetting
    {
        struct Value
        {
            char string[20];

            bool operator==(const Value& other) const
            {
                return std::memcmp(string, other.string, sizeof(string)) == 0;
            }
        };

        struct Properties
        {
            static constexpr auto  KEY           = misc::StringLiteral{ "StringSetting" };
            static constexpr Value DEFAULT_VALUE = { .string = "DefaultString" };
            static constexpr bool  KEEP_ON_RESET = false;

            static constexpr bool validate(const Value& v)
            {
                return sizeof(v) <= sizeof(Value::string);
            }
        };
    };

    struct TransientSetting
    {
        struct Value
        {
            uint16_t value;

            bool operator==(const Value&) const = default;
        };

        struct Properties
        {
            static constexpr auto  KEY           = misc::StringLiteral{ "TransientSetting" };
            static constexpr Value DEFAULT_VALUE = { .value = 7 };
            static constexpr bool  KEEP_ON_RESET = false;
        };
    };

    using TestSettingsManager = settings::SettingsManager<misc::StringLiteral{ "testing" },
                                                          ScalarSettingKeepOnReset,
                                                          ScalarSettingNoKeepOnReset,
                                                          StringSetting,
                                                          TransientSetting>;
}    // namespace

class SettingsTest : public Test
{
    public:
    SettingsTest()  = default;
    ~SettingsTest() = default;

    protected:
    void SetUp() override
    {
        LOG_INF("Running test %s", UnitTest::GetInstance()->current_test_info()->name());
        ASSERT_EQ(0, settings_subsys_init());
        clear_settings_backend_records();
    }

    void TearDown() override
    {
        LOG_INF("Test %s complete", UnitTest::GetInstance()->current_test_info()->name());
    }

    void write_settings_backend_record(const char* path, const void* value, size_t size)
    {
        ASSERT_EQ(0, settings_subsys_init());
        ASSERT_EQ(0, settings_save_one(path, value, size));
    }

    void clear_settings_backend_records()
    {
        static constexpr std::array<const char*, 6> PATHS = {
            "testing/ScalarSettingKeepOnReset",
            "testing/ScalarSettingNoKeepOnReset",
            "testing/StringSetting",
            "testing/TransientSetting",
            "testing/UnknownSetting",
            "testing/ScalarSettingKeepOnReset/child",
        };

        for (const auto* path : PATHS)
        {
            ASSERT_EQ(0, settings_delete(path));
        }
    }
};

TEST_F(SettingsTest, ReadDefaultValues)
{
    TestSettingsManager settings_manager;

    ASSERT_TRUE(settings_manager.init());
    ASSERT_EQ(ScalarSettingKeepOnReset::Properties::DEFAULT_VALUE, settings_manager.value<ScalarSettingKeepOnReset>());
    ASSERT_EQ(ScalarSettingNoKeepOnReset::Properties::DEFAULT_VALUE, settings_manager.value<ScalarSettingNoKeepOnReset>());
    ASSERT_EQ(StringSetting::Properties::DEFAULT_VALUE, settings_manager.value<StringSetting>());
    ASSERT_EQ(TransientSetting::Properties::DEFAULT_VALUE, settings_manager.value<TransientSetting>());
}

TEST_F(SettingsTest, WriteAndReadScalarSettingSuccess)
{
    TestSettingsManager settings_manager;

    ScalarSettingKeepOnReset::Value value_keep{
        .channel   = 10,
        .frequency = 1234,
        .enabled   = false,
    };

    ScalarSettingNoKeepOnReset::Value value_no_keep{
        .channel   = 10,
        .frequency = 1234,
        .enabled   = false,
    };

    ASSERT_TRUE(settings_manager.init());
    ASSERT_TRUE(ScalarSettingKeepOnReset::Properties::validate(value_keep));
    ASSERT_TRUE(ScalarSettingNoKeepOnReset::Properties::validate(value_no_keep));
    ASSERT_TRUE(settings_manager.update<ScalarSettingKeepOnReset>(value_keep));
    ASSERT_TRUE(settings_manager.update<ScalarSettingNoKeepOnReset>(value_no_keep));

    ASSERT_EQ(value_keep, settings_manager.value<ScalarSettingKeepOnReset>());
    ASSERT_EQ(value_no_keep, settings_manager.value<ScalarSettingNoKeepOnReset>());

    TestSettingsManager reloaded_manager;

    ASSERT_TRUE(reloaded_manager.init());
    ASSERT_EQ(value_keep, reloaded_manager.value<ScalarSettingKeepOnReset>());
    ASSERT_EQ(value_no_keep, reloaded_manager.value<ScalarSettingNoKeepOnReset>());

    // Perform a factory reset.
    ASSERT_TRUE(reloaded_manager.reset());

    TestSettingsManager reset_manager;

    ASSERT_TRUE(reset_manager.init());

    // ScalarSettingKeepOnReset has keep on reset flag, the value should be unchanged even after the factory reset.
    ASSERT_EQ(value_keep, reset_manager.value<ScalarSettingKeepOnReset>());

    // ScalarSettingNoKeepOnReset has no keep on reset flag, the value should be reverted to the default one.
    ASSERT_EQ(ScalarSettingNoKeepOnReset::Properties::DEFAULT_VALUE, reset_manager.value<ScalarSettingNoKeepOnReset>());
}

TEST_F(SettingsTest, WriteAndReadScalarSettingFailure)
{
    TestSettingsManager settings_manager;

    ScalarSettingNoKeepOnReset::Value value_invalid{
        .channel   = 100,
        .frequency = 0,
        .enabled   = true,
    };

    ASSERT_TRUE(settings_manager.init());
    EXPECT_FALSE(ScalarSettingNoKeepOnReset::Properties::validate(value_invalid));
    EXPECT_FALSE(settings_manager.update<ScalarSettingNoKeepOnReset>(value_invalid));
    EXPECT_EQ(ScalarSettingNoKeepOnReset::Properties::DEFAULT_VALUE, settings_manager.value<ScalarSettingNoKeepOnReset>());
}

TEST_F(SettingsTest, WriteAndReadStringSettingSuccess)
{
    TestSettingsManager settings_manager;

    StringSetting::Value value{
        .string = "newnewnewnew",
    };

    ASSERT_TRUE(StringSetting::Properties::validate(value));
    ASSERT_TRUE(settings_manager.init());
    ASSERT_TRUE(settings_manager.update<StringSetting>(value));
    ASSERT_EQ(value, settings_manager.value<StringSetting>());

    TestSettingsManager reloaded_manager;

    ASSERT_TRUE(reloaded_manager.init());
    ASSERT_EQ(value, reloaded_manager.value<StringSetting>());
}

TEST_F(SettingsTest, WriteWithoutRetentionDoesNotPersistAcrossManagerInstances)
{
    TestSettingsManager settings_manager;

    TransientSetting::Value value{
        .value = 42,
    };

    ASSERT_TRUE(settings_manager.init());
    ASSERT_TRUE(settings_manager.update<TransientSetting>(value, false));
    ASSERT_EQ(value, settings_manager.value<TransientSetting>());

    TestSettingsManager reloaded_manager;

    ASSERT_TRUE(reloaded_manager.init());
    ASSERT_EQ(TransientSetting::Properties::DEFAULT_VALUE, reloaded_manager.value<TransientSetting>());
}

TEST_F(SettingsTest, InitIgnoresUnknownAndNestedStoredSettings)
{
    ScalarSettingKeepOnReset::Value stored_value{
        .channel   = 9,
        .frequency = 123456,
        .enabled   = false,
    };

    write_settings_backend_record("testing/UnknownSetting", &stored_value, sizeof(stored_value));
    write_settings_backend_record("testing/ScalarSettingKeepOnReset/child", &stored_value, sizeof(stored_value));

    TestSettingsManager settings_manager;

    ASSERT_TRUE(settings_manager.init());
    EXPECT_EQ(ScalarSettingKeepOnReset::Properties::DEFAULT_VALUE, settings_manager.value<ScalarSettingKeepOnReset>());
    EXPECT_EQ(ScalarSettingNoKeepOnReset::Properties::DEFAULT_VALUE, settings_manager.value<ScalarSettingNoKeepOnReset>());
    EXPECT_EQ(StringSetting::Properties::DEFAULT_VALUE, settings_manager.value<StringSetting>());
    EXPECT_EQ(TransientSetting::Properties::DEFAULT_VALUE, settings_manager.value<TransientSetting>());
}

TEST_F(SettingsTest, InitIgnoresStoredValueWithUnexpectedSize)
{
    ScalarSettingKeepOnReset::Value stored_value{
        .channel   = 9,
        .frequency = 123456,
        .enabled   = false,
    };

    write_settings_backend_record("testing/ScalarSettingKeepOnReset", &stored_value, sizeof(stored_value) - 1);

    TestSettingsManager settings_manager;

    ASSERT_TRUE(settings_manager.init());
    EXPECT_EQ(ScalarSettingKeepOnReset::Properties::DEFAULT_VALUE, settings_manager.value<ScalarSettingKeepOnReset>());
}
