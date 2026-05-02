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

#include <array>
#include <optional>
#include <span>

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
        static constexpr size_t PAGE_1_SIZE      = PAGE_SIZE;
        static constexpr size_t PAGE_2_SIZE      = PAGE_SIZE;
        static constexpr size_t FACTORY_SIZE     = PAGE_SIZE;
        static constexpr size_t WRITE_BLOCK_SIZE = sizeof(uint32_t);

        bool init() override
        {
            return device_is_ready(_page1.dev) && device_is_ready(_page2.dev) && device_is_ready(_factory.dev);
        }

        bool erase_page(Page page) override
        {
            const auto& partition = partition_for(page);
            return flash_erase(partition.dev, partition.offset, partition.size) == 0;
        }

        bool write(Page page, uint32_t offset, std::span<const uint8_t> data) override
        {
            const auto& partition = partition_for(page);

            if ((data.size() != WRITE_BLOCK_SIZE) ||
                ((offset % WRITE_BLOCK_SIZE) != 0) ||
                ((offset + data.size()) > partition.size))
            {
                return false;
            }

            return flash_write(partition.dev, partition.offset + offset, data.data(), data.size()) == 0;
        }

        bool read(Page page, uint32_t offset, std::span<uint8_t> data) override
        {
            const auto& partition = partition_for(page);

            if ((data.empty()) ||
                ((data.size() % WRITE_BLOCK_SIZE) != 0) ||
                ((offset % WRITE_BLOCK_SIZE) != 0) ||
                ((offset + data.size()) > partition.size))
            {
                return false;
            }

            return flash_read(partition.dev, partition.offset + offset, data.data(), data.size()) == 0;
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

        static inline const Partition _page1   = make_partition(PARTITION_DEVICE(page1_partition),
                                                                PARTITION_OFFSET(page1_partition),
                                                                PARTITION_SIZE(page1_partition));
        static inline const Partition _page2   = make_partition(PARTITION_DEVICE(page2_partition),
                                                                PARTITION_OFFSET(page2_partition),
                                                                PARTITION_SIZE(page2_partition));
        static inline const Partition _factory = make_partition(PARTITION_DEVICE(factory_partition),
                                                                PARTITION_OFFSET(factory_partition),
                                                                PARTITION_SIZE(factory_partition));
    };

    class EmuEepromFlashTest : public ::testing::Test
    {
        protected:
        void SetUp() override
        {
            ASSERT_GE(PARTITION_SIZE(page1_partition), PAGE_SIZE);
            ASSERT_GE(PARTITION_SIZE(page2_partition), PAGE_SIZE);
            ASSERT_GE(PARTITION_SIZE(factory_partition), PAGE_SIZE);

            ASSERT_TRUE(_hwa.init());
            ASSERT_TRUE(_hwa.erase_page(Page::Page1));
            ASSERT_TRUE(_hwa.erase_page(Page::Page2));
            ASSERT_TRUE(_hwa.erase_page(Page::Factory));
        }

        void write_raw_flash_entry(Page page, size_t slot_index, uint16_t address, uint16_t value)
        {
            constexpr uint32_t                              DATA_OFFSET = HwaFlash::WRITE_BLOCK_SIZE;
            std::array<uint8_t, HwaFlash::WRITE_BLOCK_SIZE> buffer      = {};

            std::fill(buffer.begin(), buffer.end(), 0xFF);
            serialize_entry(make_entry(address, value), std::span<uint8_t>(buffer.data(), SERIALIZED_ENTRY_SIZE));
            ASSERT_TRUE(_hwa.write(page, DATA_OFFSET + (slot_index * sizeof(uint32_t)), buffer));
        }

        void write_raw_page_status(Page page, uint16_t status)
        {
            std::array<uint8_t, HwaFlash::WRITE_BLOCK_SIZE> buffer = {};

            std::fill(buffer.begin(), buffer.end(), 0xFF);
            serialize_entry(make_entry(RESERVED_STATUS_ADDRESS, status), std::span<uint8_t>(buffer.data(), SERIALIZED_ENTRY_SIZE));
            ASSERT_TRUE(_hwa.write(page, 0, buffer));
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

    for (size_t i = 0; i < PAGE_SIZE; i++)
    {
        write_value = 0x1200 + i;
        ASSERT_EQ(WriteStatus::Ok, emu_eeprom.write(make_entry(0, write_value)));

        if (emu_eeprom.page_status(Page::Page2) == PageStatus::Valid)
        {
            break;
        }
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

    write_raw_page_status(Page::Factory, static_cast<uint16_t>(PageStatus::Valid));
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
    ASSERT_EQ(WriteStatus::Ok, emu_eeprom.write(make_entry(0, 0x2468)));
    write_raw_page_status(Page::Factory, static_cast<uint16_t>(PageStatus::Formatted));

    ASSERT_FALSE(emu_eeprom.restore_from_factory());
    ASSERT_EQ(PageStatus::Valid, emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, emu_eeprom.read(0, value));
    ASSERT_EQ(0x2468, value);
}
