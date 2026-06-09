/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "tests/tests_common.h"

#include "zlibs/utils/emueeprom/emueeprom.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>

using namespace zlibs::utils::emueeprom;

namespace
{
    constexpr size_t PAGE_COUNT = 3;
    constexpr size_t PAGE_SIZE  = 256;

    template<typename HwaType>
    constexpr uint32_t data_start_offset()
    {
        return HwaType::WRITE_BLOCK_SIZE;
    }

    constexpr uint32_t raw_entry_offset(size_t slot_index)
    {
        return static_cast<uint32_t>((slot_index + 1) * SERIALIZED_ENTRY_SIZE);
    }

    void assert_entry_eq(Entry expected, Entry actual)
    {
        ASSERT_EQ(expected.address, actual.address);
        ASSERT_EQ(expected.value, actual.value);
    }

    template<size_t WriteBlockSize = sizeof(uint32_t)>
    class HwaTestBase : public Hwa
    {
        public:
        static constexpr size_t WRITE_BLOCK_SIZE = WriteBlockSize;

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

        bool write(Page page, uint32_t offset, std::span<const uint8_t> data) override
        {
            if (should_fail_write(page, offset))
            {
                return false;
            }

            auto& page_data = page_array.at(static_cast<size_t>(page));

            if ((data.size() != WRITE_BLOCK_SIZE) ||
                ((offset % WRITE_BLOCK_SIZE) != 0) ||
                ((offset + data.size()) > page_data.size()))
            {
                return false;
            }

            // 0->1 transition is not allowed on flash.
            for (size_t i = 0; i < data.size(); i++)
            {
                if ((data[i] | page_data.at(offset + i)) != page_data.at(offset + i))
                {
                    return false;
                }
            }

            std::copy(data.begin(), data.end(), page_data.begin() + offset);
            return true;
        }

        bool read(Page page, uint32_t offset, std::span<uint8_t> data) override
        {
            if (should_fail_read(page, offset))
            {
                return false;
            }

            const auto& page_data = page_array.at(static_cast<size_t>(page));

            if ((data.empty()) ||
                ((data.size() % WRITE_BLOCK_SIZE) != 0) ||
                ((offset % WRITE_BLOCK_SIZE) != 0) ||
                ((offset + data.size()) > page_data.size()))
            {
                return false;
            }

            max_read_size = std::max(max_read_size, data.size());
            std::copy(page_data.begin() + offset, page_data.begin() + offset + data.size(), data.begin());
            return true;
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

        // Writes one serialized entry directly into the backing flash array, bypassing flash rules.
        void force_write_entry(Page page, uint32_t offset, Entry entry)
        {
            auto& page_data = page_array.at(static_cast<size_t>(page));
            auto  data      = std::span<uint8_t>(page_data.data() + offset, SERIALIZED_ENTRY_SIZE);

            serialize_entry(entry, data);
        }

        // Reads one serialized entry directly from the backing flash array without failure injection.
        Entry raw_read_entry(Page page, uint32_t offset) const
        {
            const auto& page_data = page_array.at(static_cast<size_t>(page));
            const auto  data      = std::span<const uint8_t>(page_data.data() + offset, SERIALIZED_ENTRY_SIZE);

            return deserialize_entry(data);
        }

        // Reads a 32-bit word directly from the backing flash array without failure injection.
        uint32_t raw_read_32(Page page, uint32_t offset) const
        {
            const auto& page_data = page_array.at(static_cast<size_t>(page));

            return deserialize_value<uint32_t>(std::span<const uint8_t>(page_data.data() + offset, sizeof(uint32_t)));
        }

        bool                                                   fail_init          = false;
        std::optional<Page>                                    fail_erase_page    = {};
        std::optional<Page>                                    fail_read_page     = {};
        std::optional<uint32_t>                                fail_read_offset   = {};
        std::optional<Page>                                    fail_write_page    = {};
        std::optional<uint32_t>                                fail_write_offset  = {};
        std::array<std::array<uint8_t, PAGE_SIZE>, PAGE_COUNT> page_array         = {};
        size_t                                                 page_erase_counter = 0;
        size_t                                                 max_read_size      = 0;

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

    using HwaTest  = HwaTestBase<>;
    using HwaTest8 = HwaTestBase<sizeof(uint64_t)>;

    class HwaLargeFactoryTest : public Hwa
    {
        public:
        static constexpr size_t WRITE_BLOCK_SIZE  = sizeof(uint32_t);
        static constexpr size_t FACTORY_PAGE_SIZE = PAGE_SIZE * 2;

        bool init() override
        {
            return true;
        }

        bool erase_page(Page page) override
        {
            std::fill(page_storage(page).begin(), page_storage(page).end(), 0xFF);
            return true;
        }

        bool write(Page page, uint32_t offset, std::span<const uint8_t> data) override
        {
            auto storage = page_storage(page);

            if ((data.size() != WRITE_BLOCK_SIZE) ||
                ((offset % WRITE_BLOCK_SIZE) != 0) ||
                ((offset + data.size()) > storage.size()))
            {
                return false;
            }

            for (size_t i = 0; i < data.size(); i++)
            {
                if ((data[i] | storage[offset + i]) != storage[offset + i])
                {
                    return false;
                }
            }

            std::copy(data.begin(), data.end(), storage.begin() + offset);
            return true;
        }

        bool read(Page page, uint32_t offset, std::span<uint8_t> data) override
        {
            auto storage = page_storage(page);

            if ((data.empty()) ||
                ((data.size() % WRITE_BLOCK_SIZE) != 0) ||
                ((offset % WRITE_BLOCK_SIZE) != 0) ||
                ((offset + data.size()) > storage.size()))
            {
                return false;
            }

            std::copy(storage.begin() + offset, storage.begin() + offset + data.size(), data.begin());
            return true;
        }

        void erase_raw_page(Page page)
        {
            std::fill(page_storage(page).begin(), page_storage(page).end(), 0xFF);
        }

        uint8_t raw_read_byte(Page page, uint32_t offset) const
        {
            return page_storage(page)[offset];
        }

        private:
        std::span<uint8_t> page_storage(Page page)
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

        std::span<const uint8_t> page_storage(Page page) const
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

        std::array<uint8_t, PAGE_SIZE>         _page1   = {};
        std::array<uint8_t, PAGE_SIZE>         _page2   = {};
        std::array<uint8_t, FACTORY_PAGE_SIZE> _factory = {};
    };

    using EmuEepromTestStorage         = EmuEeprom<PAGE_SIZE, HwaTest::WRITE_BLOCK_SIZE>;
    using EmuEepromWriteBlockStorage   = EmuEeprom<PAGE_SIZE, HwaTest8::WRITE_BLOCK_SIZE>;
    using EmuEepromLargeFactoryStorage = EmuEeprom<PAGE_SIZE, HwaLargeFactoryTest::WRITE_BLOCK_SIZE>;

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
                _hwa.force_write_entry(page,
                                       0,
                                       make_entry(RESERVED_STATUS_ADDRESS, static_cast<uint16_t>(status)));
            }
        }

        void write_raw_flash_entry(Page page, size_t slot_index, uint16_t address, uint16_t value)
        {
            _hwa.force_write_entry(page,
                                   raw_entry_offset(slot_index),
                                   make_entry(address, value));
        }

        HwaTest              _hwa;
        EmuEepromTestStorage _emu_eeprom = EmuEepromTestStorage(_hwa);
    };

    class EmuEepromWriteBlockTest : public ::testing::Test
    {
        protected:
        void SetUp() override
        {
            _hwa.erase_raw_page(Page::Page1);
            _hwa.erase_raw_page(Page::Page2);
            _hwa.erase_raw_page(Page::Factory);
            ASSERT_TRUE(_emu_eeprom.init());
        }

        static constexpr uint32_t first_data_offset()
        {
            return data_start_offset<HwaTest8>();
        }

        HwaTest8                   _hwa;
        EmuEepromWriteBlockStorage _emu_eeprom = EmuEepromWriteBlockStorage(_hwa);
    };

    class EmuEepromLargeFactoryTest : public ::testing::Test
    {
        protected:
        void SetUp() override
        {
            _hwa.erase_raw_page(Page::Page1);
            _hwa.erase_raw_page(Page::Page2);
            _hwa.erase_raw_page(Page::Factory);
            ASSERT_TRUE(_emu_eeprom.init());
        }

        HwaLargeFactoryTest          _hwa;
        EmuEepromLargeFactoryStorage _emu_eeprom = EmuEepromLargeFactoryStorage(_hwa);
    };

}    // namespace

TEST_F(EmuEepromTest, ReadNonExisting)
{
    uint16_t value = 0;
    ASSERT_EQ(ReadStatus::NoVariable, _emu_eeprom.read(0, value));
}

TEST_F(EmuEepromTest, StartupScansUseBulkReads)
{
    HwaTest              hwa;
    EmuEepromTestStorage emu_eeprom(hwa);

    hwa.erase_raw_page(Page::Page1);
    hwa.erase_raw_page(Page::Page2);
    hwa.erase_raw_page(Page::Factory);

    ASSERT_TRUE(emu_eeprom.init());
    ASSERT_GT(hwa.max_read_size, HwaTest::WRITE_BLOCK_SIZE);
}

TEST_F(EmuEepromWriteBlockTest, FourByteBackendUsesFourByteHeader)
{
    HwaTest              hwa;
    EmuEepromTestStorage emu_eeprom(hwa);

    hwa.erase_raw_page(Page::Page1);
    hwa.erase_raw_page(Page::Page2);
    hwa.erase_raw_page(Page::Factory);

    ASSERT_TRUE(emu_eeprom.init());
    ASSERT_EQ(WriteStatus::Ok, emu_eeprom.write(make_entry(0, 0x1234)));

    assert_entry_eq(make_entry(RESERVED_STATUS_ADDRESS, static_cast<uint16_t>(PageStatus::Valid)),
                    hwa.raw_read_entry(Page::Page1, 0));
    assert_entry_eq(make_entry(0, 0x1234),
                    hwa.raw_read_entry(Page::Page1, sizeof(uint32_t)));
}

TEST_F(EmuEepromWriteBlockTest, EightByteBackendUsesFullHeaderBlock)
{
    assert_entry_eq(make_entry(RESERVED_STATUS_ADDRESS, static_cast<uint16_t>(PageStatus::Valid)),
                    _hwa.raw_read_entry(Page::Page1, 0));
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Page1, sizeof(uint32_t)));
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Page1, first_data_offset()));
}

TEST_F(EmuEepromWriteBlockTest, PacksTwoLogicalEntriesIntoOneBackendWrite)
{
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1111)));
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Page1, first_data_offset()));

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(1, 0x2222)));
    assert_entry_eq(make_entry(0, 0x1111),
                    _hwa.raw_read_entry(Page::Page1, first_data_offset()));
    assert_entry_eq(make_entry(1, 0x2222),
                    _hwa.raw_read_entry(Page::Page1, first_data_offset() + sizeof(uint32_t)));
}

TEST_F(EmuEepromWriteBlockTest, HalfFullBackendBlockIsReadableButDurableOnlyAfterFlush)
{
    uint16_t value = 0;

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1111)));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x1111, value);
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Page1, first_data_offset()));

    ASSERT_TRUE(_emu_eeprom.flush());

    assert_entry_eq(make_entry(0, 0x1111),
                    _hwa.raw_read_entry(Page::Page1, first_data_offset()));
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Page1, first_data_offset() + sizeof(uint32_t)));
}

TEST_F(EmuEepromWriteBlockTest, ReinitContinuesAfterFlushedPartialBackendBlock)
{
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1111)));
    ASSERT_TRUE(_emu_eeprom.flush());

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(1, 0x2222)));
    ASSERT_TRUE(_emu_eeprom.flush());

    assert_entry_eq(make_entry(0, 0x1111),
                    _hwa.raw_read_entry(Page::Page1, first_data_offset()));
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Page1, first_data_offset() + sizeof(uint32_t)));
    assert_entry_eq(make_entry(1, 0x2222),
                    _hwa.raw_read_entry(Page::Page1, first_data_offset() + HwaTest8::WRITE_BLOCK_SIZE));
}

TEST_F(EmuEepromWriteBlockTest, CacheOnlyWritesPersistLatestValuesOnFlush)
{
    uint16_t value = 0;

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1111), true));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(1, 0x2222), true));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x3333), true));
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Page1, first_data_offset()));

    ASSERT_TRUE(_emu_eeprom.flush());
    ASSERT_TRUE(_emu_eeprom.init());

    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x3333, value);
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(1, value));
    ASSERT_EQ(0x2222, value);
}

TEST_F(EmuEepromWriteBlockTest, StoreAndRestoreFactorySnapshotHandlePackedEntries)
{
    uint16_t value = 0;

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1111)));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(1, 0x2222)));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(2, 0x3333)));
    ASSERT_TRUE(_emu_eeprom.store_to_factory());

    ASSERT_TRUE(_emu_eeprom.format());
    ASSERT_EQ(ReadStatus::NoVariable, _emu_eeprom.read(0, value));

    ASSERT_TRUE(_emu_eeprom.restore_from_factory());

    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x1111, value);
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(1, value));
    ASSERT_EQ(0x2222, value);
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(2, value));
    ASSERT_EQ(0x3333, value);
}

TEST_F(EmuEepromWriteBlockTest, PageTransferFlushesPendingBlockBeforeSwitchingPages)
{
    uint16_t value = 0;

    const uint16_t last_address = static_cast<uint16_t>(_emu_eeprom.max_address() - 1);

    for (uint16_t address = 0; address < last_address; address++)
    {
        ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(address, static_cast<uint16_t>(address + 1))));
    }

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.page_transfer());
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(1, value);

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(1, value);
}

TEST_F(EmuEepromLargeFactoryTest, StoreAndRestoreFactorySnapshotWorkWhenFactoryPageIsLarger)
{
    uint16_t value = 0;

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1111)));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(1, 0x2222)));
    ASSERT_TRUE(_emu_eeprom.store_to_factory());

    ASSERT_TRUE(_emu_eeprom.format());
    ASSERT_EQ(ReadStatus::NoVariable, _emu_eeprom.read(0, value));

    ASSERT_TRUE(_emu_eeprom.restore_from_factory());
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x1111, value);
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(1, value));
    ASSERT_EQ(0x2222, value);
}

TEST_F(EmuEepromLargeFactoryTest, StoreToFactoryLeavesBytesBeyondRuntimePageUntouched)
{
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1111)));
    ASSERT_TRUE(_emu_eeprom.store_to_factory());

    for (uint32_t offset = PAGE_SIZE; offset < HwaLargeFactoryTest::FACTORY_PAGE_SIZE; offset++)
    {
        ASSERT_EQ(0xFF, _hwa.raw_read_byte(Page::Factory, offset));
    }
}

TEST_F(EmuEepromTest, Insert)
{
    uint16_t value = 0;

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1234)));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1235)));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1236)));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1237)));

    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x1237, value);
}

TEST_F(EmuEepromTest, PageTransfer)
{
    uint16_t value       = 0;
    uint16_t write_value = 0;

    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page2));

    for (size_t i = 0; i < PAGE_SIZE; i++)
    {
        write_value = 0x1234 + i;
        ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, write_value)));

        if (_emu_eeprom.page_status(Page::Page2) == PageStatus::Valid)
        {
            break;
        }
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

    const size_t half_page_entries = (static_cast<size_t>(_emu_eeprom.max_address()) / 2) - 1;

    for (size_t i = 0; i < half_page_entries; i++)
    {
        ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(static_cast<uint16_t>(i), 0)));
    }

    for (size_t i = 0; i < half_page_entries; i++)
    {
        uint16_t value = 0;

        ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(i, value));
        ASSERT_EQ(0, value);
    }

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.page_transfer());

    for (size_t i = 0; i < half_page_entries; i++)
    {
        ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(static_cast<uint16_t>(i), 1)));
    }

    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));

    for (size_t i = 0; i < half_page_entries; i++)
    {
        uint16_t value = 0;

        ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(i, value));
        ASSERT_EQ(1, value);
    }

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));

    for (size_t i = 0; i < half_page_entries; i++)
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
    write_raw_flash_entry(Page::Page1, 0, static_cast<uint16_t>(PAGE_SIZE + 1), 0x0000u);

    assert_entry_eq(make_entry(static_cast<uint16_t>(PAGE_SIZE + 1), 0x0000u),
                    _hwa.raw_read_entry(Page::Page1, data_start_offset<HwaTest>()));

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Page1, data_start_offset<HwaTest>()));

    ASSERT_EQ(WriteStatus::WriteError, _emu_eeprom.write(make_entry(static_cast<uint16_t>(max_address), 0)));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(static_cast<uint16_t>(max_address - 1), 0)));
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

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1234), true));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1235), true));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1236), true));
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1237), true));

    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x1237, value);

    ASSERT_TRUE(_emu_eeprom.init());
    ASSERT_EQ(ReadStatus::NoVariable, _emu_eeprom.read(0, value));

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1237), true));
    ASSERT_TRUE(_emu_eeprom.flush());

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

    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x4321)));
    set_page_status(Page::Factory, PageStatus::Formatted);

    ASSERT_FALSE(_emu_eeprom.restore_from_factory());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::Ok, _emu_eeprom.read(0, value));
    ASSERT_EQ(0x4321, value);
}

TEST_F(EmuEepromTest, FormatPreservesFactoryPageUnlessRequested)
{
    set_page_status(Page::Factory, PageStatus::Valid);
    write_raw_flash_entry(Page::Factory, 0, 0, 0x1234);

    ASSERT_TRUE(_emu_eeprom.format());
    ASSERT_EQ(PageStatus::Valid, _emu_eeprom.page_status(Page::Factory));
    assert_entry_eq(make_entry(0, 0x1234),
                    _hwa.raw_read_entry(Page::Factory, data_start_offset<HwaTest>()));

    ASSERT_TRUE(_emu_eeprom.format(true));
    ASSERT_EQ(PageStatus::Erased, _emu_eeprom.page_status(Page::Factory));
    ASSERT_EQ(0xFFFFFFFFu, _hwa.raw_read_32(Page::Factory, data_start_offset<HwaTest>()));
}

TEST_F(EmuEepromTest, ReadWriteAndTransferFailWhenNoValidPageExists)
{
    uint16_t value = 0;

    set_page_status(Page::Page1, PageStatus::Formatted);
    set_page_status(Page::Page2, PageStatus::Formatted);

    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page1));
    ASSERT_EQ(PageStatus::Formatted, _emu_eeprom.page_status(Page::Page2));
    ASSERT_EQ(ReadStatus::NoPage, _emu_eeprom.read(0, value));
    ASSERT_EQ(WriteStatus::NoPage, _emu_eeprom.write(make_entry(0, 0x1111)));
    ASSERT_EQ(WriteStatus::NoPage, _emu_eeprom.page_transfer());
}

TEST_F(EmuEepromTest, RestoreFromFactoryFailsWhenFactoryCopyReadFails)
{
    set_page_status(Page::Factory, PageStatus::Valid);
    write_raw_flash_entry(Page::Factory, 0, 0, 0x1234);

    _hwa.fail_read_page   = Page::Factory;
    _hwa.fail_read_offset = data_start_offset<HwaTest>();

    ASSERT_FALSE(_emu_eeprom.restore_from_factory());
}

TEST_F(EmuEepromTest, PageTransferPropagatesNewPageHeaderWriteFailure)
{
    ASSERT_EQ(WriteStatus::Ok, _emu_eeprom.write(make_entry(0, 0x1234)));

    _hwa.fail_write_page   = Page::Page2;
    _hwa.fail_write_offset = HwaTest::WRITE_BLOCK_SIZE;

    ASSERT_EQ(WriteStatus::WriteError, _emu_eeprom.page_transfer());
}
