/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/emueeprom/emueeprom.h"

#include <algorithm>

using namespace zlibs::utils::emueeprom;

bool EmuEeprom::init()
{
    if (!_hwa.init())
    {
        return false;
    }

    bool do_cache = true;

    _next_offset_to_write = 0;
    std::fill(_cache.begin(), _cache.end(), ERASED_VALUE);

    const auto PAGE_1_STATUS = page_status(Page::Page1);
    const auto PAGE_2_STATUS = page_status(Page::Page2);

    /*
     * Repair any invalid page-header combination left behind by reset or
     * interrupted page transfers before exposing the EEPROM to callers.
     */
    switch (PAGE_1_STATUS)
    {
    case PageStatus::Erased:
    {
        if (PAGE_2_STATUS == PageStatus::Valid)
        {
            if (!_hwa.erase_page(Page::Page1))
            {
                return false;
            }

            if (!_hwa.write_32(Page::Page1, 0, static_cast<uint32_t>(PageStatus::Formatted)))
            {
                return false;
            }
        }
        else
        {
            if (!format())
            {
                return false;
            }

            do_cache = false;
        }
    }
    break;

    case PageStatus::Receiving:
    {
        if (PAGE_2_STATUS == PageStatus::Valid)
        {
            if (!_hwa.erase_page(Page::Page1))
            {
                return false;
            }

            if (!cache())
            {
                if (!format())
                {
                    return false;
                }

                do_cache = false;
                break;
            }

            if (page_transfer() != WriteStatus::Ok)
            {
                if (!format())
                {
                    return false;
                }
            }

            do_cache = false;
        }
        else
        {
            if (!format())
            {
                return false;
            }

            do_cache = false;
        }
    }
    break;

    case PageStatus::Valid:
    {
        if (PAGE_2_STATUS == PageStatus::Valid)
        {
            if (!format())
            {
                return false;
            }
        }
        else if (PAGE_2_STATUS == PageStatus::Erased)
        {
            if (!_hwa.erase_page(Page::Page2))
            {
                return false;
            }

            if (!_hwa.write_32(Page::Page2, 0, static_cast<uint32_t>(PageStatus::Formatted)))
            {
                return false;
            }
        }
        else if (PAGE_2_STATUS == PageStatus::Formatted)
        {
            // Already in the expected steady state.
        }
        else if (PAGE_2_STATUS == PageStatus::Receiving)
        {
            if (!_hwa.erase_page(Page::Page2))
            {
                return false;
            }

            if (!cache())
            {
                if (!format())
                {
                    return false;
                }

                do_cache = false;
                break;
            }

            if (page_transfer() != WriteStatus::Ok)
            {
                if (!format())
                {
                    return false;
                }
            }

            do_cache = false;
        }
        else
        {
            if (!format())
            {
                return false;
            }

            do_cache = false;
        }
    }
    break;

    case PageStatus::Formatted:
    {
        if (PAGE_2_STATUS == PageStatus::Valid)
        {
            // Already in the expected steady state.
        }
        else
        {
            if (!format())
            {
                return false;
            }

            do_cache = false;
        }
    }
    break;

    default:
    {
        if (!format())
        {
            return false;
        }

        do_cache = false;
    }
    break;
    }

    if (do_cache)
    {
        /*
         * If reconstructing the RAM mirror fails for any reason, recover by
         * reformatting the runtime pages to a known-good state.
         */
        if (!cache())
        {
            return format();
        }
    }

    return true;
}

bool EmuEeprom::format(bool format_factory_page)
{
    if (!_hwa.erase_page(Page::Page1))
    {
        return false;
    }

    if (!_hwa.erase_page(Page::Page2))
    {
        return false;
    }

    if (format_factory_page && !_hwa.erase_page(Page::Factory))
    {
        return false;
    }

    std::fill(_cache.begin(), _cache.end(), ERASED_VALUE);

    if (!_hwa.write_32(Page::Page1, 0, static_cast<uint32_t>(PageStatus::Valid)))
    {
        return false;
    }

    if (!_hwa.write_32(Page::Page2, 0, static_cast<uint32_t>(PageStatus::Formatted)))
    {
        return false;
    }

    _next_offset_to_write = 0;

    return true;
}

bool EmuEeprom::store_to_factory()
{
    Page active_page;

    if (!find_valid_page(PageOperation::Read, active_page))
    {
        return false;
    }

    if (!_hwa.erase_page(Page::Factory))
    {
        return false;
    }

    for (uint32_t offset = 0; offset < CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE; offset += ENTRY_SIZE_BYTES)
    {
        const auto FLASH_WORD = _hwa.read_32(active_page, offset);

        if (!FLASH_WORD.has_value())
        {
            return false;
        }

        if (FLASH_WORD.value() == EMPTY_WORD)
        {
            break;
        }

        if (!_hwa.write_32(Page::Factory, offset, FLASH_WORD.value()))
        {
            return false;
        }
    }

    return true;
}

bool EmuEeprom::restore_from_factory()
{
    if (page_status(Page::Factory) != PageStatus::Valid)
    {
        return false;
    }

    if (!format())
    {
        return false;
    }

    for (uint32_t offset = 0; offset < CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE; offset += ENTRY_SIZE_BYTES)
    {
        const auto FACTORY_WORD = _hwa.read_32(Page::Factory, offset);

        if (!FACTORY_WORD.has_value())
        {
            return false;
        }

        if (FACTORY_WORD.value() == EMPTY_WORD)
        {
            break;
        }

        if (!_hwa.write_32(Page::Page1, offset, FACTORY_WORD.value()))
        {
            return false;
        }
    }

    if (!_hwa.write_32(Page::Page2, 0, static_cast<uint32_t>(PageStatus::Formatted)))
    {
        return false;
    }

    _next_offset_to_write = 0;

    return cache();
}

ReadStatus EmuEeprom::read(uint32_t address, uint16_t& data)
{
    if (address >= max_address())
    {
        return ReadStatus::ReadError;
    }

    if (_cache[address] != ERASED_VALUE)
    {
        data = _cache[address];
        return ReadStatus::Ok;
    }

    Page valid_page;

    if (!find_valid_page(PageOperation::Read, valid_page))
    {
        return ReadStatus::NoPage;
    }

    uint32_t read_offset = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE - ENTRY_SIZE_BYTES;

    if (_next_offset_to_write >= ENTRY_SIZE_BYTES)
    {
        /*
         * `_next_offset_to_write` points at the next free slot in the active
         * page, so one entry earlier is the most recent written word.
         */
        read_offset = _next_offset_to_write - ENTRY_SIZE_BYTES;
    }

    while (read_offset >= PAGE_HEADER_SIZE)
    {
        auto retrieved = _hwa.read_32(valid_page, read_offset);

        if (retrieved.has_value())
        {
            if (flash_entry_address(retrieved.value()) == address)
            {
                _cache[address] = flash_entry_value(retrieved.value());
                data            = _cache[address];
                return ReadStatus::Ok;
            }
        }

        read_offset -= ENTRY_SIZE_BYTES;
    }

    return ReadStatus::NoVariable;
}

WriteStatus EmuEeprom::write(uint32_t address, uint16_t data, bool cache_only)
{
    if (address >= max_address())
    {
        return WriteStatus::WriteError;
    }

    auto status = write_internal(static_cast<uint16_t>(address), data, cache_only);

    if (status == WriteStatus::PageFull)
    {
        status = page_transfer();

        if (status == WriteStatus::Ok)
        {
            status = write_internal(static_cast<uint16_t>(address), data);
        }
    }

    return status;
}

bool EmuEeprom::find_valid_page(PageOperation operation, Page& page)
{
    const auto PAGE_1_STATUS = page_status(Page::Page1);
    const auto PAGE_2_STATUS = page_status(Page::Page2);

    switch (operation)
    {
    case PageOperation::Write:
    {
        if (PAGE_2_STATUS == PageStatus::Valid)
        {
            page = (PAGE_1_STATUS == PageStatus::Receiving) ? Page::Page1 : Page::Page2;
        }
        else if (PAGE_1_STATUS == PageStatus::Valid)
        {
            page = (PAGE_2_STATUS == PageStatus::Receiving) ? Page::Page2 : Page::Page1;
        }
        else
        {
            return false;
        }
    }
    break;

    case PageOperation::Read:
    {
        if (PAGE_1_STATUS == PageStatus::Valid)
        {
            page = Page::Page1;
        }
        else if (PAGE_2_STATUS == PageStatus::Valid)
        {
            page = Page::Page2;
        }
        else
        {
            return false;
        }
    }
    break;

    default:
    {
        page = Page::Page1;
    }
    break;
    }

    return true;
}

WriteStatus EmuEeprom::write_internal(uint16_t address, uint16_t data, bool cache_only)
{
    if (address == ERASED_VALUE)
    {
        return WriteStatus::WriteError;
    }

    if (cache_only)
    {
        _cache[address] = data;
        return WriteStatus::Ok;
    }

    Page valid_page;

    if (!find_valid_page(PageOperation::Write, valid_page))
    {
        return WriteStatus::NoPage;
    }

    uint32_t write_offset = PAGE_HEADER_SIZE;

    if (_next_offset_to_write != 0)
    {
        if (_next_offset_to_write >= CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE)
        {
            return WriteStatus::PageFull;
        }

        if (!_hwa.write_32(valid_page, _next_offset_to_write, make_flash_entry(address, data)))
        {
            return WriteStatus::WriteError;
        }

        _next_offset_to_write += ENTRY_SIZE_BYTES;
        _cache[address] = data;
        return WriteStatus::Ok;
    }

    while (write_offset < CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE)
    {
        auto read_data = _hwa.read_32(valid_page, write_offset);

        if (read_data.has_value())
        {
            if (read_data.value() == EMPTY_WORD)
            {
                if (!_hwa.write_32(valid_page, write_offset, make_flash_entry(address, data)))
                {
                    return WriteStatus::WriteError;
                }

                _next_offset_to_write = write_offset + ENTRY_SIZE_BYTES;
                _cache[address]       = data;
                return WriteStatus::Ok;
            }
        }

        write_offset += ENTRY_SIZE_BYTES;
    }

    return WriteStatus::PageFull;
}

WriteStatus EmuEeprom::page_transfer()
{
    Page old_page;
    Page new_page = Page::Page1;

    if (!find_valid_page(PageOperation::Read, old_page))
    {
        return WriteStatus::NoPage;
    }

    if (old_page == Page::Page2)
    {
        new_page = Page::Page1;
    }
    else if (old_page == Page::Page1)
    {
        new_page = Page::Page2;
    }
    else
    {
        return WriteStatus::NoPage;
    }

    if (!_hwa.write_32(new_page, 0, static_cast<uint32_t>(PageStatus::Receiving)))
    {
        return WriteStatus::WriteError;
    }

    _next_offset_to_write = PAGE_HEADER_SIZE;

    /*
     * Because the latest value for every logical address is already mirrored in
     * RAM, page transfer becomes a straight cache dump into the new page.
     */
    for (size_t address = 0; address < ADDRESS_COUNT; address++)
    {
        if (_cache[address] == ERASED_VALUE)
        {
            continue;
        }

        const auto STATUS = write_internal(static_cast<uint16_t>(address), _cache[address]);

        if (STATUS != WriteStatus::Ok)
        {
            return STATUS;
        }
    }

    if (!_hwa.erase_page(old_page))
    {
        return WriteStatus::WriteError;
    }

    if (!_hwa.write_32(old_page, 0, static_cast<uint32_t>(PageStatus::Formatted)))
    {
        return WriteStatus::WriteError;
    }

    if (!_hwa.write_32(new_page, 0, static_cast<uint32_t>(PageStatus::Valid)))
    {
        return WriteStatus::WriteError;
    }

    return WriteStatus::Ok;
}

PageStatus EmuEeprom::page_status(Page page)
{
    auto data = std::optional<uint32_t>{};

    switch (page)
    {
    case Page::Page1:
    {
        data = _hwa.read_32(Page::Page1, 0);

        if (!data.has_value())
        {
            return PageStatus::Erased;
        }
    }
    break;

    case Page::Page2:
    {
        data = _hwa.read_32(Page::Page2, 0);

        if (!data.has_value())
        {
            return PageStatus::Erased;
        }
    }
    break;

    case Page::Factory:
    {
        data = _hwa.read_32(Page::Factory, 0);

        if (!data.has_value())
        {
            return PageStatus::Erased;
        }
    }
    break;

    default:
        return PageStatus::Erased;
    }

    switch (data.value())
    {
    case static_cast<uint32_t>(PageStatus::Erased):
        return PageStatus::Erased;

    case static_cast<uint32_t>(PageStatus::Receiving):
        return PageStatus::Receiving;

    case static_cast<uint32_t>(PageStatus::Valid):
        return PageStatus::Valid;

    default:
        return PageStatus::Formatted;
    }
}

bool EmuEeprom::cache()
{
    Page valid_page;

    std::fill(_cache.begin(), _cache.end(), ERASED_VALUE);

    if (!find_valid_page(PageOperation::Write, valid_page))
    {
        return false;
    }

    for (uint32_t offset = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE - ENTRY_SIZE_BYTES;
         offset >= PAGE_HEADER_SIZE;
         offset -= ENTRY_SIZE_BYTES)
    {
        auto data = _hwa.read_32(valid_page, offset);

        if (!data.has_value())
        {
            continue;
        }

        if (data.value() == EMPTY_WORD)
        {
            continue;
        }

        const uint16_t VALUE   = flash_entry_value(data.value());
        const uint16_t ADDRESS = flash_entry_address(data.value());

        if (ADDRESS >= max_address())
        {
            return false;
        }

        if (_cache[ADDRESS] != ERASED_VALUE)
        {
            continue;
        }

        _cache[ADDRESS] = VALUE;
    }

    return true;
}

void EmuEeprom::write_cache_to_flash()
{
    (void)page_transfer();
}
