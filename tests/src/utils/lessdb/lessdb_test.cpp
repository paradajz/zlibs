/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/lessdb/lessdb.h"
#include "zlibs/utils/threads/threads.h"

using namespace zlibs::utils::lessdb;

namespace
{
    using DbThread1 = zlibs::utils::threads::UserThread<zlibs::utils::misc::StringLiteral{ "db-thread-1" }, K_PRIO_PREEMPT(0), 2048>;
    using DbThread2 = zlibs::utils::threads::UserThread<zlibs::utils::misc::StringLiteral{ "db-thread-2" }, K_PRIO_PREEMPT(0), 2048>;

    static constexpr int CONCURRENT_ITERATIONS = 50;
    static constexpr int CONCURRENT_TIMEOUT_MS = 5000;

    class DatabaseTest : public ::testing::Test
    {
        public:
        static constexpr uint32_t LESSDB_SIZE = 1021;

        protected:
        void SetUp() override
        {
            ASSERT_EQ(_lessdb.db_size(), LESSDB_SIZE);
            ASSERT_TRUE(_lessdb.set_layout(TEST_LAYOUT));
            ASSERT_TRUE(_lessdb.init_data(FactoryResetType::Full));
        }

        void TearDown() override
        {}

        class HwaLessDb : public Hwa
        {
            public:
            HwaLessDb()
            {
                _readCallback = [this](uint32_t address, SectionParameterType type)
                {
                    return memory_read(address, type);
                };

                _writeCallback = [this](uint32_t address, uint32_t value, SectionParameterType type)
                {
                    return memory_write(address, value, type);
                };
            }

            bool init() override
            {
                return true;
            }

            uint32_t size() override
            {
                return DatabaseTest::LESSDB_SIZE;
            }

            bool clear() override
            {
                memset(_memoryArray, 0, DatabaseTest::LESSDB_SIZE);
                return true;
            }

            std::optional<uint32_t> read(uint32_t address, SectionParameterType type) override
            {
                return _readCallback(address, type);
            }

            bool write(uint32_t address, uint32_t value, SectionParameterType type) override
            {
                return _writeCallback(address, value, type);
            }

            std::optional<uint32_t> memory_read_fail(uint32_t address, SectionParameterType type)
            {
                return {};
            }

            bool memory_write_fail(uint32_t address, uint32_t value, SectionParameterType type)
            {
                return false;
            }

            std::optional<uint32_t> memory_read(uint32_t address, SectionParameterType type)
            {
                uint32_t value = 0;

                switch (type)
                {
                case SectionParameterType::Bit:
                case SectionParameterType::Byte:
                case SectionParameterType::HalfByte:
                {
                    value = _memoryArray[address];
                }
                break;

                case SectionParameterType::Word:
                {
                    value = _memoryArray[address + 1];
                    value <<= 8;
                    value |= _memoryArray[address + 0];
                }
                break;

                default:
                {
                    // case SectionParameterType::Dword:
                    value = _memoryArray[address + 3];
                    value <<= 8;
                    value |= _memoryArray[address + 2];
                    value <<= 8;
                    value |= _memoryArray[address + 1];
                    value <<= 8;
                    value |= _memoryArray[address + 0];
                }
                break;
                }

                return value;
            }

            bool memory_write(uint32_t address, uint32_t value, SectionParameterType type)
            {
                switch (type)
                {
                case SectionParameterType::Bit:
                case SectionParameterType::Byte:
                case SectionParameterType::HalfByte:
                {
                    _memoryArray[address] = value;
                }
                break;

                case SectionParameterType::Word:
                {
                    _memoryArray[address + 0] = (value >> 0) & (uint16_t)0xFF;
                    _memoryArray[address + 1] = (value >> 8) & (uint16_t)0xFF;
                }
                break;

                case SectionParameterType::Dword:
                {
                    _memoryArray[address + 0] = (value >> 0) & (uint32_t)0xFF;
                    _memoryArray[address + 1] = (value >> 8) & (uint32_t)0xFF;
                    _memoryArray[address + 2] = (value >> 16) & (uint32_t)0xFF;
                    _memoryArray[address + 3] = (value >> 24) & (uint32_t)0xFF;
                }
                break;
                }

                return true;
            }

            std::function<std::optional<uint32_t>(uint32_t address, SectionParameterType type)> _readCallback;
            std::function<bool(uint32_t address, uint32_t value, SectionParameterType type)>    _writeCallback;

            private:
            uint8_t _memoryArray[DatabaseTest::LESSDB_SIZE];
        };

        static constexpr size_t TEST_BLOCK_INDEX = 0;

        static constexpr size_t SECTION_PARAMS[] = {
            5,
            10,
            15,
            10,
            15,
            10,
        };

        static constexpr SectionParameterType SECTION_TYPES[] = {
            SectionParameterType::Bit,
            SectionParameterType::Byte,
            SectionParameterType::HalfByte,
            SectionParameterType::Word,
            SectionParameterType::Dword,
            SectionParameterType::Byte,
        };

        static constexpr uint32_t DEFAULT_VALUES[] = {
            1,
            10,
            15,
            20,
            25,
            30,
        };

        static constexpr auto BLOCK_0 = make_block(std::array<Section, 6>{
            Section{
                SECTION_PARAMS[0],
                SECTION_TYPES[0],
                PreserveSetting::Disable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[0],
            },
            Section{
                SECTION_PARAMS[1],
                SECTION_TYPES[1],
                PreserveSetting::Enable,
                AutoIncrementSetting::Enable,
                DEFAULT_VALUES[1],
            },
            Section{
                SECTION_PARAMS[2],
                SECTION_TYPES[2],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[2],
            },
            Section{
                SECTION_PARAMS[3],
                SECTION_TYPES[3],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[3],
            },
            Section{
                SECTION_PARAMS[4],
                SECTION_TYPES[4],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[4],
            },
            Section{
                SECTION_PARAMS[5],
                SECTION_TYPES[5],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[5],
            },
        });

        static constexpr auto BLOCK_1 = make_block(std::array<Section, 6>{
            Section{
                SECTION_PARAMS[1],
                SECTION_TYPES[1],
                PreserveSetting::Disable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[1],
            },
            Section{
                SECTION_PARAMS[2],
                SECTION_TYPES[2],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[2],
            },
            Section{
                SECTION_PARAMS[3],
                SECTION_TYPES[3],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[3],
            },
            Section{
                SECTION_PARAMS[4],
                SECTION_TYPES[4],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[4],
            },
            Section{
                SECTION_PARAMS[5],
                SECTION_TYPES[5],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[5],
            },
            Section{
                SECTION_PARAMS[0],
                SECTION_TYPES[0],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[0],
            },
        });

        static constexpr auto BLOCK_2 = make_block(std::array<Section, 6>{
            Section{
                SECTION_PARAMS[2],
                SECTION_TYPES[2],
                PreserveSetting::Disable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[2],
            },
            Section{
                SECTION_PARAMS[3],
                SECTION_TYPES[3],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[3],
            },
            Section{
                SECTION_PARAMS[4],
                SECTION_TYPES[4],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[4],
            },
            Section{
                SECTION_PARAMS[5],
                SECTION_TYPES[5],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[5],
            },
            Section{
                SECTION_PARAMS[0],
                SECTION_TYPES[0],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[0],
            },
            Section{
                SECTION_PARAMS[1],
                SECTION_TYPES[1],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[1],
            },
        });

        static constexpr auto BLOCK_3 = make_block(std::array<Section, 6>{
            Section{
                SECTION_PARAMS[3],
                SECTION_TYPES[3],
                PreserveSetting::Disable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[3],
            },
            Section{
                SECTION_PARAMS[4],
                SECTION_TYPES[4],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[4],
            },
            Section{
                SECTION_PARAMS[5],
                SECTION_TYPES[5],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[5],
            },
            Section{
                SECTION_PARAMS[0],
                SECTION_TYPES[0],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[0],
            },
            Section{
                SECTION_PARAMS[1],
                SECTION_TYPES[1],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[1],
            },
            Section{
                SECTION_PARAMS[2],
                SECTION_TYPES[2],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[2],
            },
        });

        static constexpr auto BLOCK_4 = make_block(std::array<Section, 6>{
            Section{
                SECTION_PARAMS[4],
                SECTION_TYPES[4],
                PreserveSetting::Disable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[4],
            },
            Section{
                SECTION_PARAMS[5],
                SECTION_TYPES[5],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[5],
            },
            Section{
                SECTION_PARAMS[0],
                SECTION_TYPES[0],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[0],
            },
            Section{
                SECTION_PARAMS[1],
                SECTION_TYPES[1],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[1],
            },
            Section{
                SECTION_PARAMS[2],
                SECTION_TYPES[2],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[2],
            },
            Section{
                SECTION_PARAMS[3],
                SECTION_TYPES[3],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[3],
            },
        });

        static constexpr auto BLOCK_5 = make_block(std::array<Section, 6>{
            Section{
                SECTION_PARAMS[5],
                SECTION_TYPES[5],
                PreserveSetting::Disable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[5],
            },
            Section{
                SECTION_PARAMS[0],
                SECTION_TYPES[0],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[0],
            },
            Section{
                SECTION_PARAMS[1],
                SECTION_TYPES[1],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[1],
            },
            Section{
                SECTION_PARAMS[2],
                SECTION_TYPES[2],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[2],
            },
            Section{
                SECTION_PARAMS[3],
                SECTION_TYPES[3],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[3],
            },
            Section{
                SECTION_PARAMS[4],
                SECTION_TYPES[4],
                PreserveSetting::Enable,
                AutoIncrementSetting::Disable,
                DEFAULT_VALUES[4],
            },
        });

        static constexpr auto TEST_LAYOUT = make_layout(std::array<Block, 6>{
            Block{ BLOCK_0 },
            Block{ BLOCK_1 },
            Block{ BLOCK_2 },
            Block{ BLOCK_3 },
            Block{ BLOCK_4 },
            Block{ BLOCK_5 },
        });

        HwaLessDb _hwa;
        LessDb    _lessdb = LessDb(_hwa);
    };

}    // namespace

TEST_F(DatabaseTest, Read)
{
    uint32_t value;

    // bit section
    for (size_t i = 0; i < SECTION_PARAMS[0]; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 0, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(DEFAULT_VALUES[0], value);
    }

    // byte section
    // autoincrement is enabled for this section
    for (size_t i = 0; i < SECTION_PARAMS[1]; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(DEFAULT_VALUES[1] + i, value);
    }

    // half-byte section
    for (size_t i = 0; i < SECTION_PARAMS[2]; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 2, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(DEFAULT_VALUES[2], value);
    }

    // word section
    for (size_t i = 0; i < SECTION_PARAMS[3]; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 3, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(DEFAULT_VALUES[3], value);
    }

    // ::DWORD section
    for (size_t i = 0; i < SECTION_PARAMS[4]; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 4, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(DEFAULT_VALUES[4], value);
    }

    // try reading directly
    value = _lessdb.read(TEST_BLOCK_INDEX, 1, 0).value();
    ASSERT_EQ(DEFAULT_VALUES[1], value);

    // perform the same round of tests with different starting point
    ASSERT_TRUE(_lessdb.set_layout(TEST_LAYOUT, 100));
    _lessdb.init_data(FactoryResetType::Full);

    // bit section
    for (size_t i = 0; i < SECTION_PARAMS[0]; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 0, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(DEFAULT_VALUES[0], value);
    }

    // byte section
    // autoincrement is enabled for this section
    for (size_t i = 0; i < SECTION_PARAMS[1]; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(DEFAULT_VALUES[1] + i, value);
    }

    // half-byte section
    for (size_t i = 0; i < SECTION_PARAMS[2]; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 2, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(DEFAULT_VALUES[2], value);
    }

    // word section
    for (size_t i = 0; i < SECTION_PARAMS[3]; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 3, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(DEFAULT_VALUES[3], value);
    }

    // try reading directly
    value = _lessdb.read(TEST_BLOCK_INDEX, 1, 0).value();
    ASSERT_EQ(DEFAULT_VALUES[1], value);
}

TEST_F(DatabaseTest, Update)
{
    uint32_t value;
    bool     ret;

    // section 0, index 0
    ret = _lessdb.update(TEST_BLOCK_INDEX, 0, 0, 1);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 0, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(1, value);

    // section 0, index 1
    ret = _lessdb.update(TEST_BLOCK_INDEX, 0, 1, 0);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 0, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(0, value);

    // section 1, index 0
    ret = _lessdb.update(TEST_BLOCK_INDEX, 1, 0, 240);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(240, value);

    // section 1, index 1
    ret = _lessdb.update(TEST_BLOCK_INDEX, 1, 1, 143);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(143, value);

    // section 2, index 0
    ret = _lessdb.update(TEST_BLOCK_INDEX, 2, 0, 4);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 2, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(4, value);

    // section 2, index 1
    ret = _lessdb.update(TEST_BLOCK_INDEX, 2, 1, 12);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 2, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(12, value);

    // section 3, index 0
    ret = _lessdb.update(TEST_BLOCK_INDEX, 3, 0, 2000);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 3, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(2000, value);

    // section 3, index 1
    ret = _lessdb.update(TEST_BLOCK_INDEX, 3, 1, 1000);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 3, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(1000, value);

    // section 4, index 0
    ret = _lessdb.update(TEST_BLOCK_INDEX, 4, 0, 3300);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 4, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(3300, value);

    // section 4, index 1
    ret = _lessdb.update(TEST_BLOCK_INDEX, 4, 1, 32000);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 4, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(32000, value);

    // section 5, index 0
    ret = _lessdb.update(TEST_BLOCK_INDEX, 5, 0, 14);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 5, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(14, value);

    // section 5, index 1
    ret = _lessdb.update(TEST_BLOCK_INDEX, 5, 1, 10);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 5, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(10, value);
}

TEST_F(DatabaseTest, ErrorCheck)
{
    bool ret;

    // read
    // try calling read with invalid parameter index
    uint32_t value;

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 0, SECTION_PARAMS[0]);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }
    ASSERT_FALSE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, SECTION_PARAMS[1]);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_FALSE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 2, SECTION_PARAMS[2]);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_FALSE(ret);

    // try calling read with invalid section
    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, TEST_LAYOUT.size(), 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_FALSE(ret);

    // try calling read with invalid block
    {
        const auto READ_OPT = _lessdb.read(TEST_LAYOUT.size(), 0, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_FALSE(ret);

    // update
    ret = _lessdb.update(TEST_BLOCK_INDEX, 0, SECTION_PARAMS[0], 1);
    ASSERT_FALSE(ret);

    ret = _lessdb.update(TEST_BLOCK_INDEX, 1, SECTION_PARAMS[1], 1);
    ASSERT_FALSE(ret);

    ret = _lessdb.update(TEST_BLOCK_INDEX, 2, SECTION_PARAMS[2], 1);
    ASSERT_FALSE(ret);

    // try to init database with too many parameters
    static constexpr auto OUT_OF_BOUNDS_SECTION = make_block(std::array<Section, 1>{
        Section{
            LESSDB_SIZE + 1,
            SectionParameterType::Byte,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            1u,
        },
    });
    static constexpr auto OUT_OF_BOUNDS_LAYOUT  = make_layout(std::array<Block, 1>{ Block{ OUT_OF_BOUNDS_SECTION } });

    ret = _lessdb.set_layout(OUT_OF_BOUNDS_LAYOUT, 0);
    ASSERT_FALSE(ret);

    // try to init database with zero blocks
    ret = _lessdb.set_layout({}, 0);
    ASSERT_FALSE(ret);

    // try to init database where start offset pushes an otherwise valid layout out of bounds
    const uint32_t LAYOUT_SIZE    = _lessdb.current_database_size();
    const uint32_t EXACT_FIT_BASE = LESSDB_SIZE - LAYOUT_SIZE;

    ret = _lessdb.set_layout(TEST_LAYOUT, EXACT_FIT_BASE);
    ASSERT_TRUE(ret);

    ret = _lessdb.set_layout(TEST_LAYOUT, EXACT_FIT_BASE + 1);
    ASSERT_FALSE(ret);
}

TEST_F(DatabaseTest, ClearDB)
{
    _lessdb.clear();

    bool     ret;
    uint32_t value;

    // verify that any read value equals 0
    // bit section
    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 0, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 0, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    // byte section
    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    // half-byte section
    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 2, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 2, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    // word section
    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 3, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 3, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    // ::DWORD section
    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 4, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 4, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);
}

TEST_F(DatabaseTest, DBsize)
{
    // test if calculated database size matches the one returned from object
    int expected_size = 0;
    int db_size       = _lessdb.current_database_size();

    for (size_t i = 0; i < TEST_LAYOUT.size(); i++)
    {
        switch (SECTION_TYPES[i])
        {
        case SectionParameterType::Bit:
        {
            expected_size += ((SECTION_PARAMS[i] % 8 != 0) + SECTION_PARAMS[i] / 8);
        }
        break;

        case SectionParameterType::Byte:
        {
            expected_size += SECTION_PARAMS[i];
        }
        break;

        case SectionParameterType::HalfByte:
        {
            expected_size += ((SECTION_PARAMS[i] % 2 != 0) + SECTION_PARAMS[i] / 2);
        }
        break;

        case SectionParameterType::Word:
        {
            expected_size += (SECTION_PARAMS[i] * 2);
        }
        break;

        case SectionParameterType::Dword:
        {
            expected_size += (SECTION_PARAMS[i] * 4);
        }
        break;
        }
    }

    // test uses blocks with same sections
    ASSERT_EQ(db_size, expected_size * TEST_LAYOUT.size());
}

TEST_F(DatabaseTest, FactoryReset)
{
    // block 0, section 1 is configured to preserve values after partial reset
    // write some values first
    bool     ret;
    uint32_t value;

    ret = _lessdb.update(0, 1, 0, 16);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(0, 1, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(16, value);

    ret = _lessdb.update(0, 1, 1, 75);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(0, 1, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(75, value);

    ret = _lessdb.update(0, 1, 2, 100);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = _lessdb.read(0, 1, 2);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(100, value);

    // now perform partial reset
    _lessdb.init_data(FactoryResetType::Partial);

    // verify that updated values are unchanged
    {
        const auto READ_OPT = _lessdb.read(0, 1, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(16, value);

    {
        const auto READ_OPT = _lessdb.read(0, 1, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(75, value);

    {
        const auto READ_OPT = _lessdb.read(0, 1, 2);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(100, value);
}

TEST_F(DatabaseTest, AutoIncrement)
{
    // block 0, section 1 has autoincrement configured
    // verify

    bool     ret;
    uint32_t value;
    uint32_t test_value;

    for (size_t i = 0; i < SECTION_PARAMS[1]; i++)
    {
        test_value = i + DEFAULT_VALUES[1];
        {
            const auto READ_OPT = _lessdb.read(0, 1, i);
            ret                 = READ_OPT.has_value();
            value               = READ_OPT.value_or(0);
        }

        ASSERT_TRUE(ret);
        ASSERT_EQ(test_value, value);
    }
}

TEST_F(DatabaseTest, LayoutUid)
{
    static constexpr auto ORDERED_SECTIONS = make_block(std::array<Section, 3>{
        Section{
            8,
            SectionParameterType::Byte,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            1u,
        },
        Section{
            4,
            SectionParameterType::Word,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            2u,
        },
        Section{
            6,
            SectionParameterType::Bit,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            0u,
        },
    });

    static constexpr auto REORDERED_SECTIONS = make_block(std::array<Section, 3>{
        Section{
            4,
            SectionParameterType::Word,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            2u,
        },
        Section{
            8,
            SectionParameterType::Byte,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            1u,
        },
        Section{
            6,
            SectionParameterType::Bit,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            0u,
        },
    });

    static constexpr auto     ORDERED_LAYOUT          = make_layout(std::array<Block, 1>{ Block{ ORDERED_SECTIONS } });
    static constexpr auto     REORDERED_LAYOUT        = make_layout(std::array<Block, 1>{ Block{ REORDERED_SECTIONS } });
    static constexpr uint32_t ORDERED_UID_CONSTEXPR   = LessDb::layout_uid(ORDERED_LAYOUT);
    static constexpr uint32_t REORDERED_UID_CONSTEXPR = LessDb::layout_uid(REORDERED_LAYOUT);

    const uint32_t ORDERED_UID     = LessDb::layout_uid(ORDERED_LAYOUT);
    const uint32_t REORDERED_UID   = LessDb::layout_uid(REORDERED_LAYOUT);
    const uint32_t ORDERED_UID_DUP = LessDb::layout_uid(ORDERED_LAYOUT);

    ASSERT_EQ(ORDERED_UID, ORDERED_UID_CONSTEXPR);
    ASSERT_EQ(REORDERED_UID, REORDERED_UID_CONSTEXPR);
    ASSERT_NE(ORDERED_UID, 0);
    ASSERT_NE(REORDERED_UID, 0);
    ASSERT_EQ(ORDERED_UID, ORDERED_UID_DUP);
    ASSERT_NE(ORDERED_UID, REORDERED_UID);
    ASSERT_EQ(LessDb::layout_uid(std::array<Block, 0>{}), 0);
}

TEST_F(DatabaseTest, FailedRead)
{
    // configure memory read callback to always return false
    _hwa._readCallback = [this](uint32_t address, SectionParameterType type)
    {
        return _hwa.memory_read_fail(address, type);
    };

    bool     ret;
    uint32_t value;

    // check if reading now returns an error for all sections
    // block 0
    for (size_t i = 0; i < TEST_LAYOUT.size(); i++)
    {
        {
            const auto READ_OPT = _lessdb.read(0, i, 0);
            ret                 = READ_OPT.has_value();
            value               = READ_OPT.value_or(0);
        }

        ASSERT_FALSE(ret);
    }
}

TEST_F(DatabaseTest, FailedWrite)
{
    // configure memory write callback to always return false
    _hwa._writeCallback =
        [this](uint32_t address, uint32_t value, SectionParameterType type)
    {
        return _hwa.memory_write_fail(address, value, type);
    };

    bool ret;

    // check if writing now returns an error for all sections
    // block 0
    for (size_t i = 0; i < TEST_LAYOUT.size(); i++)
    {
        ret = _lessdb.update(0, i, 0, 0);
        ASSERT_FALSE(ret);
    }
}

TEST_F(DatabaseTest, CachingHalfByte)
{
    // half-byte and bit parameters use caching
    // once half-byte value or bit value is read, both the address and values are stored internally
    // this is used to avoid accessing the database if not needed

    static constexpr size_t TEST_CACHING_HALFBYTE_AMOUNT_OF_PARAMS        = 4;
    static constexpr size_t TEST_CACHING_HALFBYTE_SECTION_0_DEFAULT_VALUE = 10;
    static constexpr size_t TEST_CACHING_HALFBYTE_SECTION_1_DEFAULT_VALUE = 4;
    static constexpr size_t TEST_CACHING_HALFBYTE_SECTION_2_DEFAULT_VALUE = 0;

    static constexpr auto CACHING_TEST_BLOCK_0_SECTIONS = make_block(std::array<Section, 3>{
        Section{
            TEST_CACHING_HALFBYTE_AMOUNT_OF_PARAMS,
            SectionParameterType::HalfByte,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            static_cast<uint32_t>(TEST_CACHING_HALFBYTE_SECTION_0_DEFAULT_VALUE),
        },
        Section{
            TEST_CACHING_HALFBYTE_AMOUNT_OF_PARAMS,
            SectionParameterType::HalfByte,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            static_cast<uint32_t>(TEST_CACHING_HALFBYTE_SECTION_1_DEFAULT_VALUE),
        },
        Section{
            TEST_CACHING_HALFBYTE_AMOUNT_OF_PARAMS,
            SectionParameterType::HalfByte,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            static_cast<uint32_t>(TEST_CACHING_HALFBYTE_SECTION_2_DEFAULT_VALUE),
        },
    });

    static constexpr auto CACHING_LAYOUT = make_layout(std::array<Block, 1>{ Block{ CACHING_TEST_BLOCK_0_SECTIONS } });

    ASSERT_TRUE(_lessdb.set_layout(CACHING_LAYOUT, 0));
    _lessdb.init_data(FactoryResetType::Full);

    // the simulated database is now initialized
    // create new database object which won't initialize/write data (only the layout will be set)
    // this is used so that database doesn't call ::update function which resets the cached address
    LessDb db2(_hwa);
    ASSERT_TRUE(db2.set_layout(CACHING_LAYOUT, 0));

    // read the values back
    // this will verify that the values are read properly and that caching doesn't influence the readout
    uint32_t read_value;

    for (size_t i = 0; i < TEST_CACHING_HALFBYTE_AMOUNT_OF_PARAMS; i++)
    {
        {
            const auto READ_OPT = db2.read(0, 0, i);
            ASSERT_TRUE(READ_OPT.has_value());
            read_value = READ_OPT.value();
        }

        ASSERT_EQ(TEST_CACHING_HALFBYTE_SECTION_0_DEFAULT_VALUE, read_value);
    }

    // try reading the same value twice
    LessDb db3(_hwa);
    ASSERT_TRUE(db3.set_layout(CACHING_LAYOUT, 0));

    {
        const auto READ_OPT = db3.read(0, 0, TEST_CACHING_HALFBYTE_AMOUNT_OF_PARAMS - 1);
        ASSERT_TRUE(READ_OPT.has_value());
        read_value = READ_OPT.value();
    }

    ASSERT_EQ(TEST_CACHING_HALFBYTE_SECTION_0_DEFAULT_VALUE, read_value);

    {
        const auto READ_OPT = db3.read(0, 0, TEST_CACHING_HALFBYTE_AMOUNT_OF_PARAMS - 1);
        ASSERT_TRUE(READ_OPT.has_value());
        read_value = READ_OPT.value();
    }

    ASSERT_EQ(TEST_CACHING_HALFBYTE_SECTION_0_DEFAULT_VALUE, read_value);
}

TEST_F(DatabaseTest, CachingBit)
{
    // half-byte and bit parameters use caching
    // once half-byte value or bit value is read, both the address and values are stored internally
    // this is used to avoid accessing the database if not needed

    static constexpr size_t TEST_CACHING_BIT_AMOUNT_OF_PARAMS        = 4;
    static constexpr size_t TEST_CACHING_BIT_SECTION_0_DEFAULT_VALUE = 0;
    static constexpr size_t TEST_CACHING_BIT_SECTION_1_DEFAULT_VALUE = 1;
    static constexpr size_t TEST_CACHING_BIT_SECTION_2_DEFAULT_VALUE = 1;

    static constexpr auto CACHING_TEST_BLOCK_0_SECTIONS = make_block(std::array<Section, 3>{
        Section{
            TEST_CACHING_BIT_AMOUNT_OF_PARAMS,
            SectionParameterType::Bit,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            static_cast<uint32_t>(TEST_CACHING_BIT_SECTION_0_DEFAULT_VALUE),
        },
        Section{
            TEST_CACHING_BIT_AMOUNT_OF_PARAMS,
            SectionParameterType::Bit,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            static_cast<uint32_t>(TEST_CACHING_BIT_SECTION_1_DEFAULT_VALUE),
        },
        Section{
            TEST_CACHING_BIT_AMOUNT_OF_PARAMS,
            SectionParameterType::Bit,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            static_cast<uint32_t>(TEST_CACHING_BIT_SECTION_2_DEFAULT_VALUE),
        },
    });

    static constexpr auto CACHING_LAYOUT = make_layout(std::array<Block, 1>{ Block{ CACHING_TEST_BLOCK_0_SECTIONS } });

    ASSERT_TRUE(_lessdb.set_layout(CACHING_LAYOUT, 0));
    _lessdb.init_data(FactoryResetType::Full);

    // the simulated database is now initialized
    // create new database object which won't initialize/write data (only the layout will be set)
    // this is used so that database doesn't call ::update function which resets the cached address
    LessDb db2(_hwa);
    ASSERT_TRUE(_lessdb.set_layout(CACHING_LAYOUT, 0));

    // read the values back
    // this will verify that the values are read properly and that caching doesn't influence the readout
    uint32_t read_value;

    for (size_t i = 0; i < TEST_CACHING_BIT_AMOUNT_OF_PARAMS; i++)
    {
        {
            const auto READ_OPT = _lessdb.read(0, 0, i);
            ASSERT_TRUE(READ_OPT.has_value());
            read_value = READ_OPT.value();
        }

        ASSERT_EQ(TEST_CACHING_BIT_SECTION_0_DEFAULT_VALUE, read_value);
    }

    // try reading the same value twice
    LessDb db3(_hwa);
    ASSERT_TRUE(db3.set_layout(CACHING_LAYOUT, 0));

    {
        const auto READ_OPT = db3.read(0, 1, TEST_CACHING_BIT_AMOUNT_OF_PARAMS - 1);
        ASSERT_TRUE(READ_OPT.has_value());
        read_value = READ_OPT.value();
    }

    ASSERT_EQ(TEST_CACHING_BIT_SECTION_1_DEFAULT_VALUE, read_value);

    {
        const auto READ_OPT = db3.read(0, 1, TEST_CACHING_BIT_AMOUNT_OF_PARAMS - 1);
        ASSERT_TRUE(READ_OPT.has_value());
        read_value = READ_OPT.value();
    }

    ASSERT_EQ(TEST_CACHING_BIT_SECTION_1_DEFAULT_VALUE, read_value);
}

TEST_F(DatabaseTest, ConcurrentReads)
{
    k_sem done1 = {};
    k_sem done2 = {};
    k_sem_init(&done1, 0, 1);
    k_sem_init(&done2, 0, 1);

    DbThread1 t1([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         [[maybe_unused]] const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, 0);
                     }
                     k_sem_give(&done1);
                 });

    DbThread2 t2([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         [[maybe_unused]] const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 3, 0);
                     }
                     k_sem_give(&done2);
                 });

    ASSERT_TRUE(t1.run());
    ASSERT_TRUE(t2.run());
    ASSERT_EQ(0, k_sem_take(&done1, K_MSEC(CONCURRENT_TIMEOUT_MS)));
    ASSERT_EQ(0, k_sem_take(&done2, K_MSEC(CONCURRENT_TIMEOUT_MS)));

    uint32_t value;
    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(DEFAULT_VALUES[1], value);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 3, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(DEFAULT_VALUES[3], value);
}

TEST_F(DatabaseTest, ConcurrentReadUpdate)
{
    static constexpr uint32_t UPDATE_VALUE = 200;

    k_sem done1 = {};
    k_sem done2 = {};
    k_sem_init(&done1, 0, 1);
    k_sem_init(&done2, 0, 1);

    DbThread1 reader([&]()
                     {
                         for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                         {
                             [[maybe_unused]] const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, 0);
                         }
                         k_sem_give(&done1);
                     });

    DbThread2 updater([&]()
                      {
                          for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                          {
                              _lessdb.update(TEST_BLOCK_INDEX, 1, 0, UPDATE_VALUE);
                          }
                          k_sem_give(&done2);
                      });

    ASSERT_TRUE(reader.run());
    ASSERT_TRUE(updater.run());
    ASSERT_EQ(0, k_sem_take(&done1, K_MSEC(CONCURRENT_TIMEOUT_MS)));
    ASSERT_EQ(0, k_sem_take(&done2, K_MSEC(CONCURRENT_TIMEOUT_MS)));

    uint32_t value;

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(UPDATE_VALUE, value);
}

TEST_F(DatabaseTest, ConcurrentBitUpdates)
{
    // clear both bits first
    ASSERT_TRUE(_lessdb.update(TEST_BLOCK_INDEX, 0, 0, 0));
    ASSERT_TRUE(_lessdb.update(TEST_BLOCK_INDEX, 0, 1, 0));

    k_sem done1 = {};
    k_sem done2 = {};
    k_sem_init(&done1, 0, 1);
    k_sem_init(&done2, 0, 1);

    DbThread1 t1([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         _lessdb.update(TEST_BLOCK_INDEX, 0, 0, 1);
                     }
                     k_sem_give(&done1);
                 });

    DbThread2 t2([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         _lessdb.update(TEST_BLOCK_INDEX, 0, 1, 1);
                     }
                     k_sem_give(&done2);
                 });

    ASSERT_TRUE(t1.run());
    ASSERT_TRUE(t2.run());
    ASSERT_EQ(0, k_sem_take(&done1, K_MSEC(CONCURRENT_TIMEOUT_MS)));
    ASSERT_EQ(0, k_sem_take(&done2, K_MSEC(CONCURRENT_TIMEOUT_MS)));

    uint32_t value;

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 0, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(1U, value);

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 0, 1);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(1U, value);
}

TEST_F(DatabaseTest, ConcurrentUpdatesSharedParam)
{
    static constexpr uint32_t VALUE_A = 0xAA;
    static constexpr uint32_t VALUE_B = 0x55;

    k_sem done1 = {};
    k_sem done2 = {};
    k_sem_init(&done1, 0, 1);
    k_sem_init(&done2, 0, 1);

    DbThread1 t1([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         _lessdb.update(TEST_BLOCK_INDEX, 1, 0, VALUE_A);
                     }
                     k_sem_give(&done1);
                 });

    DbThread2 t2([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         _lessdb.update(TEST_BLOCK_INDEX, 1, 0, VALUE_B);
                     }
                     k_sem_give(&done2);
                 });

    ASSERT_TRUE(t1.run());
    ASSERT_TRUE(t2.run());
    ASSERT_EQ(0, k_sem_take(&done1, K_MSEC(CONCURRENT_TIMEOUT_MS)));
    ASSERT_EQ(0, k_sem_take(&done2, K_MSEC(CONCURRENT_TIMEOUT_MS)));

    uint32_t value;

    {
        const auto READ_OPT = _lessdb.read(TEST_BLOCK_INDEX, 1, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_TRUE(value == VALUE_A || value == VALUE_B);
}
