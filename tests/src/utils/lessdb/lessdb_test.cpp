/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/emueeprom/emueeprom.h"
#include "zlibs/utils/lessdb/lessdb.h"
#include "zlibs/utils/threads/threads.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

using namespace zlibs::utils::lessdb;

namespace
{
    using namespace zlibs::utils::emueeprom;

    using DbThread1 = zlibs::utils::threads::UserThread<zlibs::utils::misc::StringLiteral{ "db-thread-1" }, K_PRIO_PREEMPT(0), 2048>;
    using DbThread2 = zlibs::utils::threads::UserThread<zlibs::utils::misc::StringLiteral{ "db-thread-2" }, K_PRIO_PREEMPT(0), 2048>;

    static constexpr int      CONCURRENT_ITERATIONS = 50;
    static constexpr int      CONCURRENT_TIMEOUT_MS = 5000;
    static constexpr uint32_t EMUEEPROM_PAGE_SIZE   = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;

    struct Partition
    {
        const struct device* dev;
        off_t                offset;
        size_t               size;
    };

    constexpr Partition make_partition(const struct device* dev, off_t offset, size_t size)
    {
        return Partition{
            .dev    = dev,
            .offset = offset,
            .size   = size,
        };
    }

    class EmuEepromFlashPageHwa : public zlibs::utils::emueeprom::Hwa
    {
        public:
        static constexpr size_t PAGE_1_SIZE  = EMUEEPROM_PAGE_SIZE;
        static constexpr size_t PAGE_2_SIZE  = EMUEEPROM_PAGE_SIZE;
        static constexpr size_t FACTORY_SIZE = EMUEEPROM_PAGE_SIZE;

        bool init() override
        {
            return device_is_ready(_page1.dev) && device_is_ready(_page2.dev) && device_is_ready(_factory.dev);
        }

        bool erase_page(Page page) override
        {
            const auto& partition = partition_for(page);
            return flash_erase(partition.dev, partition.offset, partition.size) == 0;
        }

        bool write_32(Page page, uint32_t offset, uint32_t data) override
        {
            const auto& partition = partition_for(page);

            if ((offset + sizeof(data)) > partition.size)
            {
                return false;
            }

            return flash_write(partition.dev, partition.offset + offset, &data, sizeof(data)) == 0;
        }

        std::optional<uint32_t> read_32(Page page, uint32_t offset) override
        {
            const auto& partition = partition_for(page);

            if ((offset + sizeof(uint32_t)) > partition.size)
            {
                return {};
            }

            uint32_t data = 0;

            if (flash_read(partition.dev, partition.offset + offset, &data, sizeof(data)) != 0)
            {
                return {};
            }

            return data;
        }

        private:
        static const Partition& partition_for(Page page)
        {
            switch (page)
            {
            case Page::Page1:
                return _page1;

            case Page::Page2:
                return _page2;

            case Page::Factory:
                return _factory;
            }

            return _page1;
        }

        static inline const Partition _page1   = make_partition(FIXED_PARTITION_DEVICE(page1_partition),
                                                                FIXED_PARTITION_OFFSET(page1_partition),
                                                                FIXED_PARTITION_SIZE(page1_partition));
        static inline const Partition _page2   = make_partition(FIXED_PARTITION_DEVICE(page2_partition),
                                                                FIXED_PARTITION_OFFSET(page2_partition),
                                                                FIXED_PARTITION_SIZE(page2_partition));
        static inline const Partition _factory = make_partition(FIXED_PARTITION_DEVICE(factory_partition),
                                                                FIXED_PARTITION_OFFSET(factory_partition),
                                                                FIXED_PARTITION_SIZE(factory_partition));
    };

    static std::optional<uint8_t> emueeprom_read_byte(EmuEeprom& emu_eeprom, uint32_t address)
    {
        uint16_t data = 0;

        if (emu_eeprom.read(address, data) != ReadStatus::Ok)
        {
            return {};
        }

        return static_cast<uint8_t>(data & 0xFFu);
    }

    static bool emueeprom_write_byte(EmuEeprom& emu_eeprom, uint32_t address, uint8_t value)
    {
        return emu_eeprom.write(address, value) == WriteStatus::Ok;
    }

    struct LessDbTestLayout
    {
        static constexpr uint32_t LESSDB_SIZE = 1021;

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
    };

    struct RawBufferBackend
    {
        class HwaLessDb : public zlibs::utils::lessdb::Hwa
        {
            public:
            void assert_test_requirements() const
            {}

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
                return LessDbTestLayout::LESSDB_SIZE;
            }

            bool clear() override
            {
                memset(_memoryArray, 0, LessDbTestLayout::LESSDB_SIZE);
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
            uint8_t _memoryArray[LessDbTestLayout::LESSDB_SIZE];
        };
    };

    struct EmuEepromFlashBackend
    {
        class HwaLessDb : public zlibs::utils::lessdb::Hwa
        {
            public:
            HwaLessDb()
                : _emu_eeprom(_page_hwa)
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

            void assert_test_requirements() const
            {
                ASSERT_GE(FIXED_PARTITION_SIZE(page1_partition), EMUEEPROM_PAGE_SIZE);
                ASSERT_GE(FIXED_PARTITION_SIZE(page2_partition), EMUEEPROM_PAGE_SIZE);
                ASSERT_GE(FIXED_PARTITION_SIZE(factory_partition), EMUEEPROM_PAGE_SIZE);
            }

            bool init() override
            {
                return _emu_eeprom.init();
            }

            uint32_t size() override
            {
                return LessDbTestLayout::LESSDB_SIZE;
            }

            bool clear() override
            {
                if (!_emu_eeprom.format())
                {
                    return false;
                }

                for (uint32_t address = 0; address < LessDbTestLayout::LESSDB_SIZE; address++)
                {
                    if (!emueeprom_write_byte(_emu_eeprom, address, 0))
                    {
                        return false;
                    }
                }

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
                    const auto byte = emueeprom_read_byte(_emu_eeprom, address);

                    if (!byte.has_value())
                    {
                        return {};
                    }

                    value = byte.value();
                }
                break;

                case SectionParameterType::Word:
                {
                    const auto byte0 = emueeprom_read_byte(_emu_eeprom, address + 0);
                    const auto byte1 = emueeprom_read_byte(_emu_eeprom, address + 1);

                    if (!byte0.has_value() || !byte1.has_value())
                    {
                        return {};
                    }

                    value = byte1.value();
                    value <<= 8;
                    value |= byte0.value();
                }
                break;

                default:
                {
                    const auto byte0 = emueeprom_read_byte(_emu_eeprom, address + 0);
                    const auto byte1 = emueeprom_read_byte(_emu_eeprom, address + 1);
                    const auto byte2 = emueeprom_read_byte(_emu_eeprom, address + 2);
                    const auto byte3 = emueeprom_read_byte(_emu_eeprom, address + 3);

                    if (!byte0.has_value() || !byte1.has_value() || !byte2.has_value() || !byte3.has_value())
                    {
                        return {};
                    }

                    value = byte3.value();
                    value <<= 8;
                    value |= byte2.value();
                    value <<= 8;
                    value |= byte1.value();
                    value <<= 8;
                    value |= byte0.value();
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
                    return emueeprom_write_byte(_emu_eeprom, address, static_cast<uint8_t>(value & 0xFFu));
                }

                case SectionParameterType::Word:
                {
                    return emueeprom_write_byte(_emu_eeprom, address + 0, static_cast<uint8_t>((value >> 0) & 0xFFu)) &&
                           emueeprom_write_byte(_emu_eeprom, address + 1, static_cast<uint8_t>((value >> 8) & 0xFFu));
                }

                case SectionParameterType::Dword:
                {
                    return emueeprom_write_byte(_emu_eeprom, address + 0, static_cast<uint8_t>((value >> 0) & 0xFFu)) &&
                           emueeprom_write_byte(_emu_eeprom, address + 1, static_cast<uint8_t>((value >> 8) & 0xFFu)) &&
                           emueeprom_write_byte(_emu_eeprom, address + 2, static_cast<uint8_t>((value >> 16) & 0xFFu)) &&
                           emueeprom_write_byte(_emu_eeprom, address + 3, static_cast<uint8_t>((value >> 24) & 0xFFu));
                }
                }

                return false;
            }

            std::function<std::optional<uint32_t>(uint32_t address, SectionParameterType type)> _readCallback;
            std::function<bool(uint32_t address, uint32_t value, SectionParameterType type)>    _writeCallback;

            private:
            EmuEepromFlashPageHwa _page_hwa;
            EmuEeprom             _emu_eeprom;
        };
    };

    template<typename Backend>
    class LessDbBackendsTest : public ::testing::Test, public LessDbTestLayout
    {
        protected:
        using HwaLessDb = typename Backend::HwaLessDb;

        void SetUp() override
        {
            this->_hwa.assert_test_requirements();
            ASSERT_TRUE(this->_lessdb.init());
            ASSERT_EQ(this->_lessdb.db_size(), LessDbTestLayout::LESSDB_SIZE);
            ASSERT_TRUE(this->_lessdb.set_layout(LessDbTestLayout::TEST_LAYOUT));
            ASSERT_TRUE(this->_lessdb.init_data(FactoryResetType::Full));
        }

        HwaLessDb _hwa;
        LessDb    _lessdb = LessDb(_hwa);
    };

    using LessDbBackends = ::testing::Types<RawBufferBackend, EmuEepromFlashBackend>;
    TYPED_TEST_SUITE(LessDbBackendsTest, LessDbBackends);

}    // namespace

TYPED_TEST(LessDbBackendsTest, Read)
{
    uint32_t value;

    // Bit section.
    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[0]; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 0, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(TestFixture::DEFAULT_VALUES[0], value);
    }

    // Byte section. Autoincrement is enabled for this section.
    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[1]; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(TestFixture::DEFAULT_VALUES[1] + i, value);
    }

    // Half-byte section.
    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[2]; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 2, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(TestFixture::DEFAULT_VALUES[2], value);
    }

    // Word section.
    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[3]; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 3, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(TestFixture::DEFAULT_VALUES[3], value);
    }

    // Dword section.
    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[4]; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 4, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(TestFixture::DEFAULT_VALUES[4], value);
    }

    // Try reading directly.
    value = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 0).value();
    ASSERT_EQ(TestFixture::DEFAULT_VALUES[1], value);

    // Perform the same round of tests with a different starting point.
    ASSERT_TRUE(this->_lessdb.set_layout(TestFixture::TEST_LAYOUT, 100));
    this->_lessdb.init_data(FactoryResetType::Full);

    // Bit section.
    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[0]; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 0, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(TestFixture::DEFAULT_VALUES[0], value);
    }

    // Byte section. Autoincrement is enabled for this section.
    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[1]; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(TestFixture::DEFAULT_VALUES[1] + i, value);
    }

    // Half-byte section.
    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[2]; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 2, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(TestFixture::DEFAULT_VALUES[2], value);
    }

    // Word section.
    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[3]; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 3, i);
            ASSERT_TRUE(READ_OPT.has_value());
            value = READ_OPT.value();
        }

        ASSERT_EQ(TestFixture::DEFAULT_VALUES[3], value);
    }

    // Try reading directly.
    value = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 0).value();
    ASSERT_EQ(TestFixture::DEFAULT_VALUES[1], value);
}

TYPED_TEST(LessDbBackendsTest, Update)
{
    uint32_t value;
    bool     ret;

    // Section 0, index 0.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 0, 0, 1);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 0, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(1, value);

    // Section 0, index 1.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 0, 1, 0);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 0, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(0, value);

    // Section 1, index 0.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 1, 0, 240);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(240, value);

    // Section 1, index 1.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 1, 1, 143);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(143, value);

    // Section 2, index 0.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 2, 0, 4);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 2, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(4, value);

    // Section 2, index 1.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 2, 1, 12);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 2, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(12, value);

    // Section 3, index 0.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 3, 0, 2000);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 3, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(2000, value);

    // Section 3, index 1.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 3, 1, 1000);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 3, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(1000, value);

    // Section 4, index 0.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 4, 0, 3300);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 4, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(3300, value);

    // Section 4, index 1.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 4, 1, 32000);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 4, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(32000, value);

    // Section 5, index 0.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 5, 0, 14);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 5, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(14, value);

    // Section 5, index 1.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 5, 1, 10);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 5, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(10, value);
}

TYPED_TEST(LessDbBackendsTest, ErrorCheck)
{
    bool ret;

    // Read path: try calling `read()` with an invalid parameter index.
    uint32_t value;

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 0, TestFixture::SECTION_PARAMS[0]);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }
    ASSERT_FALSE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, TestFixture::SECTION_PARAMS[1]);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_FALSE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 2, TestFixture::SECTION_PARAMS[2]);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_FALSE(ret);

    // Try calling `read()` with an invalid section.
    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, TestFixture::TEST_LAYOUT.size(), 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_FALSE(ret);

    // Try calling `read()` with an invalid block.
    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_LAYOUT.size(), 0, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_FALSE(ret);

    // Update path.
    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 0, TestFixture::SECTION_PARAMS[0], 1);
    ASSERT_FALSE(ret);

    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 1, TestFixture::SECTION_PARAMS[1], 1);
    ASSERT_FALSE(ret);

    ret = this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 2, TestFixture::SECTION_PARAMS[2], 1);
    ASSERT_FALSE(ret);

    // Try initializing the database with too many parameters.
    static constexpr auto OUT_OF_BOUNDS_SECTION = make_block(std::array<Section, 1>{
        Section{
            TestFixture::LESSDB_SIZE + 1,
            SectionParameterType::Byte,
            PreserveSetting::Disable,
            AutoIncrementSetting::Disable,
            1u,
        },
    });
    static constexpr auto OUT_OF_BOUNDS_LAYOUT  = make_layout(std::array<Block, 1>{ Block{ OUT_OF_BOUNDS_SECTION } });

    ret = this->_lessdb.set_layout(OUT_OF_BOUNDS_LAYOUT, 0);
    ASSERT_FALSE(ret);

    // Try initializing the database with zero blocks.
    ret = this->_lessdb.set_layout({}, 0);
    ASSERT_FALSE(ret);

    // Try initializing the database with a start offset that pushes a valid layout out of bounds.
    const uint32_t LAYOUT_SIZE    = LessDb::layout_size(TestFixture::TEST_LAYOUT);
    const uint32_t EXACT_FIT_BASE = TestFixture::LESSDB_SIZE - LAYOUT_SIZE;

    ret = this->_lessdb.set_layout(TestFixture::TEST_LAYOUT, EXACT_FIT_BASE);
    ASSERT_TRUE(ret);

    ret = this->_lessdb.set_layout(TestFixture::TEST_LAYOUT, EXACT_FIT_BASE + 1);
    ASSERT_FALSE(ret);
}

TYPED_TEST(LessDbBackendsTest, ClearDB)
{
    this->_lessdb.clear();

    bool     ret;
    uint32_t value;

    // Verify that every read value equals `0` in the bit section.
    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 0, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 0, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    // Byte section.
    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    // Half-byte section.
    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 2, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 2, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    // Word section.
    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 3, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 3, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    // Dword section.
    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 4, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 4, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_EQ(0, value);
    ASSERT_TRUE(ret);
}

TYPED_TEST(LessDbBackendsTest, DBsize)
{
    // Verify that the calculated database size matches the value returned by the object.
    int expected_size = 0;
    int db_size       = LessDb::layout_size(TestFixture::TEST_LAYOUT);

    for (size_t i = 0; i < TestFixture::TEST_LAYOUT.size(); i++)
    {
        switch (TestFixture::SECTION_TYPES[i])
        {
        case SectionParameterType::Bit:
        {
            expected_size += ((TestFixture::SECTION_PARAMS[i] % 8 != 0) + TestFixture::SECTION_PARAMS[i] / 8);
        }
        break;

        case SectionParameterType::Byte:
        {
            expected_size += TestFixture::SECTION_PARAMS[i];
        }
        break;

        case SectionParameterType::HalfByte:
        {
            expected_size += ((TestFixture::SECTION_PARAMS[i] % 2 != 0) + TestFixture::SECTION_PARAMS[i] / 2);
        }
        break;

        case SectionParameterType::Word:
        {
            expected_size += (TestFixture::SECTION_PARAMS[i] * 2);
        }
        break;

        case SectionParameterType::Dword:
        {
            expected_size += (TestFixture::SECTION_PARAMS[i] * 4);
        }
        break;
        }
    }

    // The test uses blocks with the same sections.
    ASSERT_EQ(db_size, expected_size * TestFixture::TEST_LAYOUT.size());
}

TYPED_TEST(LessDbBackendsTest, FactoryReset)
{
    /*
     * Block 0, section 1 is configured to preserve values after a partial
     * reset, so write some values first.
     */
    bool     ret;
    uint32_t value;

    ret = this->_lessdb.update(0, 1, 0, 16);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(0, 1, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(16, value);

    ret = this->_lessdb.update(0, 1, 1, 75);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(0, 1, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(75, value);

    ret = this->_lessdb.update(0, 1, 2, 100);
    ASSERT_TRUE(ret);

    {
        const auto READ_OPT = this->_lessdb.read(0, 1, 2);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(100, value);

    // Perform a partial reset.
    this->_lessdb.init_data(FactoryResetType::Partial);

    // Verify that the updated values are unchanged.
    {
        const auto READ_OPT = this->_lessdb.read(0, 1, 0);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(16, value);

    {
        const auto READ_OPT = this->_lessdb.read(0, 1, 1);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(75, value);

    {
        const auto READ_OPT = this->_lessdb.read(0, 1, 2);
        ret                 = READ_OPT.has_value();
        value               = READ_OPT.value_or(0);
    }

    ASSERT_TRUE(ret);
    ASSERT_EQ(100, value);
}

TYPED_TEST(LessDbBackendsTest, AutoIncrement)
{
    // Block 0, section 1 has autoincrement configured. Verify the generated values.

    bool     ret;
    uint32_t value;
    uint32_t test_value;

    for (size_t i = 0; i < TestFixture::SECTION_PARAMS[1]; i++)
    {
        test_value = i + TestFixture::DEFAULT_VALUES[1];
        {
            const auto READ_OPT = this->_lessdb.read(0, 1, i);
            ret                 = READ_OPT.has_value();
            value               = READ_OPT.value_or(0);
        }

        ASSERT_TRUE(ret);
        ASSERT_EQ(test_value, value);
    }
}

TYPED_TEST(LessDbBackendsTest, LayoutUid)
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

TYPED_TEST(LessDbBackendsTest, FailedRead)
{
    // Configure the memory-read callback to always return `false`.
    this->_hwa._readCallback = [this](uint32_t address, SectionParameterType type)
    {
        return this->_hwa.memory_read_fail(address, type);
    };

    bool     ret;
    uint32_t value;

    // Check whether reads now fail for all sections in block `0`.
    for (size_t i = 0; i < TestFixture::TEST_LAYOUT.size(); i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(0, i, 0);
            ret                 = READ_OPT.has_value();
            value               = READ_OPT.value_or(0);
        }

        ASSERT_FALSE(ret);
    }
}

TYPED_TEST(LessDbBackendsTest, FailedWrite)
{
    // Configure the memory-write callback to always return `false`.
    this->_hwa._writeCallback =
        [this](uint32_t address, uint32_t value, SectionParameterType type)
    {
        return this->_hwa.memory_write_fail(address, value, type);
    };

    bool ret;

    // Check whether writes now fail for all sections in block `0`.
    for (size_t i = 0; i < TestFixture::TEST_LAYOUT.size(); i++)
    {
        ret = this->_lessdb.update(0, i, 0, 0);
        ASSERT_FALSE(ret);
    }
}

TYPED_TEST(LessDbBackendsTest, CachingHalfByte)
{
    /*
     * Half-byte and bit parameters use caching. Once a half-byte or bit value
     * is read, both the address and values are stored internally to avoid
     * unnecessary database access.
     */

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

    ASSERT_TRUE(this->_lessdb.set_layout(CACHING_LAYOUT, 0));
    this->_lessdb.init_data(FactoryResetType::Full);

    /*
     * The simulated database is now initialized. Create a new database object
     * that will not initialize or write data, so only the layout is set. This
     * avoids calling `::update()`, which resets the cached address.
     */
    LessDb db2(this->_hwa);
    ASSERT_TRUE(db2.set_layout(CACHING_LAYOUT, 0));

    // Read the values back to verify that they are returned correctly and that caching does not affect the readout.
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

    // Try reading the same value twice.
    LessDb db3(this->_hwa);
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

TYPED_TEST(LessDbBackendsTest, CachingBit)
{
    /*
     * Half-byte and bit parameters use caching. Once a half-byte or bit value
     * is read, both the address and values are stored internally to avoid
     * unnecessary database access.
     */

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

    ASSERT_TRUE(this->_lessdb.set_layout(CACHING_LAYOUT, 0));
    this->_lessdb.init_data(FactoryResetType::Full);

    /*
     * The simulated database is now initialized. Create a new database object
     * that will not initialize or write data, so only the layout is set. This
     * avoids calling `::update()`, which resets the cached address.
     */
    LessDb db2(this->_hwa);
    ASSERT_TRUE(db2.set_layout(CACHING_LAYOUT, 0));

    // Read the values back to verify that they are returned correctly and that caching does not affect the readout.
    uint32_t read_value;

    for (size_t i = 0; i < TEST_CACHING_BIT_AMOUNT_OF_PARAMS; i++)
    {
        {
            const auto READ_OPT = this->_lessdb.read(0, 0, i);
            ASSERT_TRUE(READ_OPT.has_value());
            read_value = READ_OPT.value();
        }

        ASSERT_EQ(TEST_CACHING_BIT_SECTION_0_DEFAULT_VALUE, read_value);
    }

    // Try reading the same value twice.
    LessDb db3(this->_hwa);
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

TYPED_TEST(LessDbBackendsTest, ConcurrentReads)
{
    k_sem done1 = {};
    k_sem done2 = {};
    k_sem_init(&done1, 0, 1);
    k_sem_init(&done2, 0, 1);

    DbThread1 t1([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         [[maybe_unused]] const auto READ_OPT =
                             this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 0);
                     }
                     k_sem_give(&done1);
                 });

    DbThread2 t2([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         [[maybe_unused]] const auto READ_OPT =
                             this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 3, 0);
                     }
                     k_sem_give(&done2);
                 });

    ASSERT_TRUE(t1.run());
    ASSERT_TRUE(t2.run());
    ASSERT_EQ(0, k_sem_take(&done1, K_MSEC(CONCURRENT_TIMEOUT_MS)));
    ASSERT_EQ(0, k_sem_take(&done2, K_MSEC(CONCURRENT_TIMEOUT_MS)));

    uint32_t value;
    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(TestFixture::DEFAULT_VALUES[1], value);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 3, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(TestFixture::DEFAULT_VALUES[3], value);
}

TYPED_TEST(LessDbBackendsTest, ConcurrentReadUpdate)
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
                             [[maybe_unused]] const auto READ_OPT =
                                 this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 0);
                         }
                         k_sem_give(&done1);
                     });

    DbThread2 updater([&]()
                      {
                          for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                          {
                              this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 1, 0, UPDATE_VALUE);
                          }
                          k_sem_give(&done2);
                      });

    ASSERT_TRUE(reader.run());
    ASSERT_TRUE(updater.run());
    ASSERT_EQ(0, k_sem_take(&done1, K_MSEC(CONCURRENT_TIMEOUT_MS)));
    ASSERT_EQ(0, k_sem_take(&done2, K_MSEC(CONCURRENT_TIMEOUT_MS)));

    uint32_t value;

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(UPDATE_VALUE, value);
}

TYPED_TEST(LessDbBackendsTest, ConcurrentBitUpdates)
{
    // Clear both bits first.
    ASSERT_TRUE(this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 0, 0, 0));
    ASSERT_TRUE(this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 0, 1, 0));

    k_sem done1 = {};
    k_sem done2 = {};
    k_sem_init(&done1, 0, 1);
    k_sem_init(&done2, 0, 1);

    DbThread1 t1([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 0, 0, 1);
                     }
                     k_sem_give(&done1);
                 });

    DbThread2 t2([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 0, 1, 1);
                     }
                     k_sem_give(&done2);
                 });

    ASSERT_TRUE(t1.run());
    ASSERT_TRUE(t2.run());
    ASSERT_EQ(0, k_sem_take(&done1, K_MSEC(CONCURRENT_TIMEOUT_MS)));
    ASSERT_EQ(0, k_sem_take(&done2, K_MSEC(CONCURRENT_TIMEOUT_MS)));

    uint32_t value;

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 0, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(1U, value);

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 0, 1);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_EQ(1U, value);
}

TYPED_TEST(LessDbBackendsTest, ConcurrentUpdatesSharedParam)
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
                         this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 1, 0, VALUE_A);
                     }
                     k_sem_give(&done1);
                 });

    DbThread2 t2([&]()
                 {
                     for (int i = 0; i < CONCURRENT_ITERATIONS; i++)
                     {
                         this->_lessdb.update(TestFixture::TEST_BLOCK_INDEX, 1, 0, VALUE_B);
                     }
                     k_sem_give(&done2);
                 });

    ASSERT_TRUE(t1.run());
    ASSERT_TRUE(t2.run());
    ASSERT_EQ(0, k_sem_take(&done1, K_MSEC(CONCURRENT_TIMEOUT_MS)));
    ASSERT_EQ(0, k_sem_take(&done2, K_MSEC(CONCURRENT_TIMEOUT_MS)));

    uint32_t value;

    {
        const auto READ_OPT = this->_lessdb.read(TestFixture::TEST_BLOCK_INDEX, 1, 0);
        ASSERT_TRUE(READ_OPT.has_value());
        value = READ_OPT.value();
    }

    ASSERT_TRUE(value == VALUE_A || value == VALUE_B);
}
