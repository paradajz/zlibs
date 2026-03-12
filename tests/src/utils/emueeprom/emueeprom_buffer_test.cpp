/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/emueeprom/emueeprom.h"

#include <array>
#include <limits>
#include <optional>

using namespace zlibs::utils::emueeprom;

namespace
{
    constexpr size_t   PAGE_COUNT                = 3;
    constexpr size_t   ENTRY_SIZE_BYTES          = sizeof(PageStatus);
    constexpr size_t   PAGE_HEADER_SIZE          = sizeof(PageStatus);
    constexpr uint32_t FLASH_ENTRY_ADDRESS_SHIFT = std::numeric_limits<uint16_t>::digits;

    class HwaTest : public Hwa
    {
        public:
        static constexpr size_t PAGE_1_SIZE  = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;
        static constexpr size_t PAGE_2_SIZE  = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;
        static constexpr size_t FACTORY_SIZE = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;

        bool init() override
        {
            return !fail_init;
        }

        bool erase_page(Page page) override
        {
            if (fail_erase_page.has_value() && (*fail_erase_page == page))
            {
                return false;
            }

            erase_raw_page(page);
            page_erase_counter++;

            return true;
        }

        bool write_32(Page page, uint32_t offset, uint32_t data) override
        {
            if (should_fail_write(page, offset))
            {
                return false;
            }

            // 0->1 transition is not allowed on flash.
            if (data > raw_read_32(page, offset))
            {
                return false;
            }

            force_write_32(page, offset, data);
            return true;
        }

        std::optional<uint32_t> read_32(Page page, uint32_t offset) override
        {
            if (should_fail_read(page, offset))
            {
                return {};
            }

            return raw_read_32(page, offset);
        }

        void clear_failures()
        {
            fail_init         = false;
            fail_erase_page   = {};
            fail_read_page    = {};
            fail_read_offset  = {};
            fail_write_page   = {};
            fail_write_offset = {};
        }

        void erase_raw_page(Page page)
        {
            std::fill(page_array.at(static_cast<size_t>(page)).begin(),
                      page_array.at(static_cast<size_t>(page)).end(),
                      0xFF);
        }

        // Writes a 32-bit word directly into the backing flash array, bypassing flash rules
        void force_write_32(Page page, uint32_t offset, uint32_t data)
        {
            auto& page_data = page_array.at(static_cast<size_t>(page));

            page_data.at(offset + 0) = static_cast<uint8_t>((data >> 0) & 0xFFu);
            page_data.at(offset + 1) = static_cast<uint8_t>((data >> 8) & 0xFFu);
            page_data.at(offset + 2) = static_cast<uint8_t>((data >> 16) & 0xFFu);
            page_data.at(offset + 3) = static_cast<uint8_t>((data >> 24) & 0xFFu);
        }

        // Reads a 32-bit word directly from the backing flash array without failure injection
        uint32_t raw_read_32(Page page, uint32_t offset) const
        {
            const auto& PAGE_DATA = page_array.at(static_cast<size_t>(page));

            uint32_t data = PAGE_DATA.at(offset + 3);
            data <<= 8;
            data |= PAGE_DATA.at(offset + 2);
            data <<= 8;
            data |= PAGE_DATA.at(offset + 1);
            data <<= 8;
            data |= PAGE_DATA.at(offset + 0);

            return data;
        }

        bool                                                                                fail_init          = false;
        std::optional<Page>                                                                 fail_erase_page    = {};
        std::optional<Page>                                                                 fail_read_page     = {};
        std::optional<uint32_t>                                                             fail_read_offset   = {};
        std::optional<Page>                                                                 fail_write_page    = {};
        std::optional<uint32_t>                                                             fail_write_offset  = {};
        std::array<std::array<uint8_t, CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE>, PAGE_COUNT> page_array         = {};
        size_t                                                                              page_erase_counter = 0;

        private:
        bool should_fail_read(Page page, uint32_t offset) const
        {
            return fail_read_page.has_value() && (*fail_read_page == page) &&
                   (!fail_read_offset.has_value() || (*fail_read_offset == offset));
        }

        bool should_fail_write(Page page, uint32_t offset) const
        {
            return fail_write_page.has_value() && (*fail_write_page == page) &&
                   (!fail_write_offset.has_value() || (*fail_write_offset == offset));
        }
    };

    class EmuEepromTest : public ::testing::Test
    {
        protected:
        void SetUp() override
        {
            reset_all_pages();
            ASSERT_TRUE(_emu_eeprom.init());
            _hwa.clear_failures();
            _hwa.page_erase_counter = 0;
        }

        void reset_all_pages()
        {
            _hwa.erase_raw_page(Page::Page1);
            _hwa.erase_raw_page(Page::Page2);
            _hwa.erase_raw_page(Page::Factory);
        }

        void set_page_status(Page page, PageStatus status)
        {
            _hwa.erase_raw_page(page);

            if (status != PageStatus::Erased)
            {
                _hwa.force_write_32(page, 0, static_cast<uint32_t>(status));
            }
        }

        void write_raw_flash_entry(Page page, size_t slot_index, uint16_t address, uint16_t value)
        {
            _hwa.force_write_32(page,
                                PAGE_HEADER_SIZE + (slot_index * ENTRY_SIZE_BYTES),
                                (static_cast<uint32_t>(address) << FLASH_ENTRY_ADDRESS_SHIFT) | value);
        }

        HwaTest   _hwa;
        EmuEeprom _emu_eeprom = EmuEeprom(_hwa);
    };

    static_assert(EmuEeprom::max_address() == ((CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / sizeof(uint32_t)) - 1));
}    // namespace

TEST_F(EmuEepromTest, ReadNonExisting)
{
    uint16_t value = 0;
    ASSERT_EQ(ReadStatus::NoVariable, _emu_eeprom.read(0, value));
}

TEST_F(EmuEepromTest, Insert)
{
    uint16_t value = 0;

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1234));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1235));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1236));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1237));

    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x1237, value);
}

TEST_F(EmuEepromTest, PageTransfer)
{
    uint16_t value       = 0;
    uint16_t write_value = 0;

    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page2));

    for (size_t i = 0; i < CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / ENTRY_SIZE_BYTES; i++)
    {
        write_value = 0x1234 + i;
        ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, write_value));
    }

    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(write_value, value);

    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));
}

TEST_F(EmuEepromTest, PageTransfer2)
{
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page2));

    for (size_t i = 0; i < (CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / ENTRY_SIZE_BYTES) / 2 - 1; i++)
    {
        ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(i, 0));
    }

    for (size_t i = 0; i < (CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / ENTRY_SIZE_BYTES) / 2 - 1; i++)
    {
        uint16_t value = 0;

        ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(i, value));
        ASSERT_EQ(0, value);
    }

    for (size_t i = 0; i < (CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / ENTRY_SIZE_BYTES) - 1; i++)
    {
        ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(i, 1));
    }

    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));

    for (size_t i = 0; i < (CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / ENTRY_SIZE_BYTES) - 1; i++)
    {
        uint16_t value = 0;

        ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(i, value));
        ASSERT_EQ(1, value);
    }

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));

    for (size_t i = 0; i < (CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE / ENTRY_SIZE_BYTES) - 1; i++)
    {
        uint16_t value = 0;

        ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(i, value));
        ASSERT_EQ(1, value);
    }
}

TEST_F(EmuEepromTest, OverFlow)
{
    uint16_t read_data16 = 0;
    auto     max_address = _emu_eeprom.max_address();

    set_page_status(Page::Page1, PageStatus::Valid);
    set_page_status(Page::Page2, PageStatus::Formatted);
    write_raw_flash_entry(Page::Page1, 0, static_cast<uint16_t>(CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE + 1), 0x0000u);

    ASSERT_EQ((static_cast<uint32_t>(CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE + 1) << FLASH_ENTRY_ADDRESS_SHIFT) | 0x0000u,
              _hwa.raw_read_32(Page::Page1, PAGE_HEADER_SIZE));

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Page1, PAGE_HEADER_SIZE));

    ASSERT_EQ(WriteStatus::WriteError, _emu_eeprom.write(max_address, 0));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(max_address - 1, 0));
    ASSERT_EQ(ReadStatus::ReadError, _emu_eeprom.read(max_address, read_data16));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(max_address - 1, read_data16));
}

TEST_F(EmuEepromTest, PageErase)
{
    ASSERT_EQ(0, _hwa.page_erase_counter);

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(0, _hwa.page_erase_counter);
}

TEST_F(EmuEepromTest, CachedWrite)
{
    uint16_t value = 0;

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1234, true));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1235, true));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1236, true));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1237, true));

    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x1237, value);

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(ReadStatus::NoVariable, _emu_eeprom.read(0, value));

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1237, true));
    _emu_eeprom.write_cache_to_flash();

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x1237, value);
}

TEST_F(EmuEepromTest, InitRepairsErasedPage1WhenPage2IsValid)
{
    uint16_t value = 0;

    set_page_status(Page::Page1, PageStatus::Erased);
    set_page_status(Page::Page2, PageStatus::Valid);
    write_raw_flash_entry(Page::Page2, 0, 0, 0x2345);

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x2345, value);
}

TEST_F(EmuEepromTest, InitRepairsErasedPage2WhenPage1IsValid)
{
    uint16_t value = 0;

    set_page_status(Page::Page1, PageStatus::Valid);
    set_page_status(Page::Page2, PageStatus::Erased);
    write_raw_flash_entry(Page::Page1, 0, 0, 0x3456);

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x3456, value);
}

TEST_F(EmuEepromTest, InitCompletesInterruptedTransferWhenPage2IsReceiving)
{
    uint16_t value = 0;

    set_page_status(Page::Page1, PageStatus::Valid);
    set_page_status(Page::Page2, PageStatus::Receiving);
    write_raw_flash_entry(Page::Page1, 0, 0, 0x4567);

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x4567, value);
}

TEST_F(EmuEepromTest, InitCompletesInterruptedTransferWhenPage1IsReceiving)
{
    uint16_t value = 0;

    set_page_status(Page::Page1, PageStatus::Receiving);
    set_page_status(Page::Page2, PageStatus::Valid);
    write_raw_flash_entry(Page::Page2, 0, 0, 0x5678);

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x5678, value);
}

TEST_F(EmuEepromTest, RestoreFromFactoryCopiesFactoryPageWhenValid)
{
    uint16_t page1_value = 0;
    uint16_t page2_value = 0;

    set_page_status(Page::Factory, PageStatus::Valid);
    write_raw_flash_entry(Page::Factory, 0, 0, 0x1234);
    write_raw_flash_entry(Page::Factory, 1, 1, 0x5678);

    ASSERT_TRUE(_emu_eeprom.restore_from_factory());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, page1_value));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(1, page2_value));
    ASSERT_EQ(0x1234, page1_value);
    ASSERT_EQ(0x5678, page2_value);
}

TEST_F(EmuEepromTest, RestoreFromFactoryFailsWhenFactoryPageIsNotValid)
{
    uint16_t value = 0;

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x4321));
    set_page_status(Page::Factory, PageStatus::Formatted);

    ASSERT_FALSE(_emu_eeprom.restore_from_factory());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x4321, value);
}

TEST_F(EmuEepromTest, FormatPreservesFactoryPageUnlessRequested)
{
    const auto factory_entry = (static_cast<uint32_t>(0) << FLASH_ENTRY_ADDRESS_SHIFT) | 0x1234u;

    set_page_status(Page::Factory, PageStatus::Valid);
    write_raw_flash_entry(Page::Factory, 0, 0, 0x1234);

    ASSERT_TRUE(_emu_eeprom.format());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Factory));
    ASSERT_EQ(factory_entry, _hwa.raw_read_32(Page::Factory, PAGE_HEADER_SIZE));

    ASSERT_TRUE(_emu_eeprom.format(true));
    ASSERT_EQ(PageStatus::Erased, _emu_eeprom.page_status(Page::Factory));
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Factory, PAGE_HEADER_SIZE));
}

TEST_F(EmuEepromTest, ReadWriteAndTransferFailWhenNoValidPageExists)
{
    uint16_t value = 0;

    set_page_status(Page::Page1, PageStatus::Formatted);
    set_page_status(Page::Page2, PageStatus::Formatted);

    ASSERT_EQ(ReadStatus::NoPage, _emu_eeprom.read(0, value));
    ASSERT_EQ(WriteStatus::NoPage, _emu_eeprom.write(0, 0x1111));
    ASSERT_EQ(WriteStatus::NoPage, _emu_eeprom.page_transfer());
}

TEST_F(EmuEepromTest, RestoreFromFactoryFailsWhenFactoryCopyReadFails)
{
    set_page_status(Page::Factory, PageStatus::Valid);
    write_raw_flash_entry(Page::Factory, 0, 0, 0x1234);

    _hwa.fail_read_page   = Page::Factory;
    _hwa.fail_read_offset = PAGE_HEADER_SIZE;

    ASSERT_FALSE(_emu_eeprom.restore_from_factory());
}

TEST_F(EmuEepromTest, PageTransferPropagatesNewPageHeaderWriteFailure)
{
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(0, 0x1234));

    _hwa.fail_write_page   = Page::Page2;
    _hwa.fail_write_offset = 0;

    ASSERT_EQ(WriteStatus::WriteError, _emu_eeprom.page_transfer());
}
