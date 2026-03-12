/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/emueeprom/emueeprom.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

#include <optional>

using namespace zlibs::utils::emueeprom;

namespace
{
    constexpr uint32_t PAGE_SIZE = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;

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

    class HwaFlash : public Hwa
    {
        public:
        static constexpr size_t PAGE_1_SIZE  = PAGE_SIZE;
        static constexpr size_t PAGE_2_SIZE  = PAGE_SIZE;
        static constexpr size_t FACTORY_SIZE = PAGE_SIZE;

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

    class EmuEepromFlashTest : public ::testing::Test
    {
        protected:
        void SetUp() override
        {
            ASSERT_GE(FIXED_PARTITION_SIZE(page1_partition), PAGE_SIZE);
            ASSERT_GE(FIXED_PARTITION_SIZE(page2_partition), PAGE_SIZE);
            ASSERT_GE(FIXED_PARTITION_SIZE(factory_partition), PAGE_SIZE);

            ASSERT_TRUE(_hwa.init());
            ASSERT_TRUE(_hwa.erase_page(Page::Page1));
            ASSERT_TRUE(_hwa.erase_page(Page::Page2));
            ASSERT_TRUE(_hwa.erase_page(Page::Factory));
        }

        void write_raw_flash_entry(Page page, size_t slot_index, uint16_t address, uint16_t value)
        {
            constexpr uint32_t ADDRESS_SHIFT = 16;
            const auto         entry         = (static_cast<uint32_t>(address) << ADDRESS_SHIFT) | value;

            ASSERT_TRUE(_hwa.write_32(page, sizeof(PageStatus) + (slot_index * sizeof(uint32_t)), entry));
        }

        HwaFlash _hwa;
    };
}    // namespace

TEST_F(EmuEepromFlashTest, PageTransferPersistsLatestValueUsingZephyrFlashApi)
{
    EmuEeprom emu_eeprom(_hwa);
    uint16_t  value       = 0;
    uint16_t  write_value = 0;

    ASSERT_TRUE(emu_eeprom.init());
    ASSERT_EQ(PageStatus::Valid, emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, emu_eeprom.page_status(Page::Page2));

    for (size_t i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++)
    {
        write_value = 0x1200 + i;
        ASSERT_EQ(WriteStatus::Ok, emu_eeprom.write(0, write_value));
    }

    ASSERT_EQ(ReadStatus::Ok, emu_eeprom.read(0, value));
    ASSERT_EQ(write_value, value);
    ASSERT_EQ(PageStatus::Formatted, emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Valid, emu_eeprom.page_status(Page::Page2));
}

TEST_F(EmuEepromFlashTest, RestoreFromFactoryCopiesFactoryPageUsingZephyrFlashApi)
{
    EmuEeprom emu_eeprom(_hwa);
    uint16_t  value0 = 0;
    uint16_t  value1 = 0;

    ASSERT_TRUE(_hwa.write_32(Page::Factory, 0, static_cast<uint32_t>(PageStatus::Valid)));
    write_raw_flash_entry(Page::Factory, 0, 0, 0x1234);
    write_raw_flash_entry(Page::Factory, 1, 1, 0x5678);

    ASSERT_TRUE(emu_eeprom.restore_from_factory());
    ASSERT_EQ(PageStatus::Valid, emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, emu_eeprom.read(0, value0));
    ASSERT_EQ(ReadStatus::Ok, emu_eeprom.read(1, value1));
    ASSERT_EQ(0x1234, value0);
    ASSERT_EQ(0x5678, value1);
}

TEST_F(EmuEepromFlashTest, RestoreFromFactoryFailsWhenFactoryPageStatusIsInvalid)
{
    EmuEeprom emu_eeprom(_hwa);
    uint16_t  value = 0;

    ASSERT_TRUE(emu_eeprom.init());
    ASSERT_EQ(WriteStatus::Ok, emu_eeprom.write(0, 0x2468));
    ASSERT_TRUE(_hwa.write_32(Page::Factory, 0, static_cast<uint32_t>(PageStatus::Formatted)));

    ASSERT_FALSE(emu_eeprom.restore_from_factory());
    ASSERT_EQ(PageStatus::Valid, emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, emu_eeprom.read(0, value));
    ASSERT_EQ(0x2468, value);
}
