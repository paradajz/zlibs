/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/emueeprom/emueeprom.h"

#include <algorithm>

using namespace zlibs::utils::emueeprom;
using namespace zlibs::utils::emueeprom::internal;

namespace
{
    /**
     * @brief Checks whether every byte is in the erased flash state.
     */
    bool all_erased(std::span<const uint8_t> data)
    {
        return std::all_of(data.begin(), data.end(), [](uint8_t byte)
                           {
                               return byte == ERASED_BYTE;
                           });
    }
}    // namespace

bool EmuEepromInternal::init()
{
    if (!_hwa.init())
    {
        return false;
    }

    bool do_cache = true;

    clear_write_block();
    set_cached_next_write_offset(Page::Page1, 0);
    _cache_dirty = false;
    std::fill(_cache.begin(), _cache.end(), ERASED_VALUE);

    if (!refresh_page_status(Page::Page1) ||
        !refresh_page_status(Page::Page2) ||
        !refresh_page_status(Page::Factory))
    {
        return false;
    }

    const auto page_1_status = cached_page_status(Page::Page1);
    const auto page_2_status = cached_page_status(Page::Page2);

    /*
     * Repair any invalid page-header combination left behind by reset or
     * interrupted page transfers before exposing the EEPROM to callers.
     */
    switch (page_1_status)
    {
    case PageStatus::Erased:
    {
        if (page_2_status == PageStatus::Valid)
        {
            if (!_hwa.erase_page(Page::Page1))
            {
                return false;
            }

            if (!write_page_status(Page::Page1, PageStatus::Formatted))
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
        if (page_2_status == PageStatus::Valid)
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
        if (page_2_status == PageStatus::Valid)
        {
            if (!format())
            {
                return false;
            }
        }
        else if (page_2_status == PageStatus::Erased)
        {
            if (!_hwa.erase_page(Page::Page2))
            {
                return false;
            }

            if (!write_page_status(Page::Page2, PageStatus::Formatted))
            {
                return false;
            }
        }
        else if (page_2_status == PageStatus::Formatted)
        {
            // Already in the expected steady state.
        }
        else if (page_2_status == PageStatus::Receiving)
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
        if (page_2_status == PageStatus::Valid)
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

bool EmuEepromInternal::format(bool format_factory_page)
{
    clear_write_block();

    if (!_hwa.erase_page(Page::Page1))
    {
        return false;
    }

    set_cached_page_status(Page::Page1, PageStatus::Erased);

    if (!_hwa.erase_page(Page::Page2))
    {
        return false;
    }

    set_cached_page_status(Page::Page2, PageStatus::Erased);

    if (format_factory_page && !_hwa.erase_page(Page::Factory))
    {
        return false;
    }

    if (format_factory_page)
    {
        set_cached_page_status(Page::Factory, PageStatus::Erased);
    }

    std::fill(_cache.begin(), _cache.end(), ERASED_VALUE);
    _cache_dirty = false;

    if (!write_page_status(Page::Page1, PageStatus::Valid))
    {
        return false;
    }

    if (!write_page_status(Page::Page2, PageStatus::Formatted))
    {
        return false;
    }

    set_cached_next_write_offset(Page::Page1, 0);

    return true;
}

bool EmuEepromInternal::store_to_factory()
{
    Page active_page;

    if (!flush())
    {
        return false;
    }

    if (!find_valid_page(PageOperation::Read, active_page))
    {
        return false;
    }

    if (!_hwa.erase_page(Page::Factory))
    {
        return false;
    }

    set_cached_page_status(Page::Factory, PageStatus::Erased);

    std::array<uint8_t, INTERNAL_BLOCK_SIZE> buffer = {};

    for (uint32_t offset = 0; offset < _page_size; offset += _write_block_size)
    {
        std::fill(buffer.begin(), buffer.end(), ERASED_BYTE);
        auto block = std::span<uint8_t>(buffer.data(), _write_block_size);

        if (!_hwa.read(active_page, offset, block))
        {
            return false;
        }

        if (all_erased(block))
        {
            break;
        }

        if (!_hwa.write(Page::Factory, offset, block))
        {
            return false;
        }
    }

    set_cached_page_status(Page::Factory, cached_page_status(active_page));

    return true;
}

bool EmuEepromInternal::restore_from_factory()
{
    if (!refresh_page_status(Page::Factory))
    {
        return false;
    }

    if (cached_page_status(Page::Factory) != PageStatus::Valid)
    {
        return false;
    }

    clear_write_block();

    if (!_hwa.erase_page(Page::Page1))
    {
        return false;
    }

    set_cached_page_status(Page::Page1, PageStatus::Erased);

    if (!_hwa.erase_page(Page::Page2))
    {
        return false;
    }

    set_cached_page_status(Page::Page2, PageStatus::Erased);

    std::fill(_cache.begin(), _cache.end(), ERASED_VALUE);
    _cache_dirty = false;

    std::array<uint8_t, INTERNAL_BLOCK_SIZE> buffer = {};

    for (uint32_t offset = 0; offset < _page_size; offset += _write_block_size)
    {
        std::fill(buffer.begin(), buffer.end(), ERASED_BYTE);
        auto block = std::span<uint8_t>(buffer.data(), _write_block_size);

        if (!_hwa.read(Page::Factory, offset, block))
        {
            return false;
        }

        if (all_erased(block))
        {
            break;
        }

        if (!_hwa.write(Page::Page1, offset, block))
        {
            return false;
        }
    }

    if (!write_page_status(Page::Page1, PageStatus::Valid))
    {
        return false;
    }

    if (!write_page_status(Page::Page2, PageStatus::Formatted))
    {
        return false;
    }

    set_cached_next_write_offset(Page::Page1, 0);

    return cache();
}

ReadStatus EmuEepromInternal::read(uint32_t address, uint16_t& data)
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

    uint32_t read_offset = _page_size - SERIALIZED_ENTRY_SIZE;

    const auto cached_read_offset = cached_next_write_offset();

    if ((cached_read_offset.page == valid_page) && (cached_read_offset.offset > 0))
    {
        /*
         * The cached next write offset points at the next free slot in the active
         * page, so one entry earlier is the most recent written word.
         */
        read_offset = cached_read_offset.offset - SERIALIZED_ENTRY_SIZE;
    }

    while (true)
    {
        auto retrieved = read_entry(valid_page, read_offset);

        if (retrieved.has_value())
        {
            if (retrieved.value().address == address)
            {
                _cache[address] = retrieved.value().value;
                data            = _cache[address];
                return ReadStatus::Ok;
            }
        }

        if (read_offset < SERIALIZED_ENTRY_SIZE)
        {
            // First data entry, nothing more to scan.
            break;
        }

        read_offset -= SERIALIZED_ENTRY_SIZE;
    }

    return ReadStatus::NoVariable;
}

WriteStatus EmuEepromInternal::write(Entry entry, bool cache_only)
{
    if ((entry.address == RESERVED_STATUS_ADDRESS) || (entry.address >= max_address()))
    {
        return WriteStatus::WriteError;
    }

    auto status = write_internal(entry, cache_only);

    if (status == WriteStatus::PageFull)
    {
        status = page_transfer();

        if (status == WriteStatus::Ok)
        {
            status = write_internal(entry);
        }
    }

    return status;
}

bool EmuEepromInternal::find_valid_page(PageOperation operation, Page& page)
{
    const auto page_1_status = cached_page_status(Page::Page1);
    const auto page_2_status = cached_page_status(Page::Page2);

    switch (operation)
    {
    case PageOperation::Write:
    {
        if (page_2_status == PageStatus::Valid)
        {
            page = (page_1_status == PageStatus::Receiving) ? Page::Page1 : Page::Page2;
        }
        else if (page_1_status == PageStatus::Valid)
        {
            page = (page_2_status == PageStatus::Receiving) ? Page::Page2 : Page::Page1;
        }
        else
        {
            return false;
        }
    }
    break;

    case PageOperation::Read:
    {
        if (page_1_status == PageStatus::Valid)
        {
            page = Page::Page1;
        }
        else if (page_2_status == PageStatus::Valid)
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

WriteStatus EmuEepromInternal::write_internal(Entry entry, bool cache_only)
{
    if ((entry.address == RESERVED_STATUS_ADDRESS) || (entry.address >= max_address()))
    {
        return WriteStatus::WriteError;
    }

    if (cache_only)
    {
        _cache[entry.address] = entry.value;
        _cache_dirty          = true;
        return WriteStatus::Ok;
    }

    Page valid_page;

    if (!find_valid_page(PageOperation::Write, valid_page))
    {
        return WriteStatus::NoPage;
    }

    uint32_t write_offset = 0;

    auto cached_write_offset = cached_next_write_offset();

    if ((cached_write_offset.page == valid_page) && (cached_write_offset.offset != 0))
    {
        if (cached_write_offset.offset >= _page_size)
        {
            return WriteStatus::PageFull;
        }

        write_offset = cached_write_offset.offset;
    }
    else
    {
        if (!refresh_next_write_offset(valid_page))
        {
            return WriteStatus::WriteError;
        }

        cached_write_offset = cached_next_write_offset();

        if (cached_write_offset.offset >= _page_size)
        {
            return WriteStatus::PageFull;
        }

        write_offset = cached_write_offset.offset;
    }

    const auto status = write_entry(valid_page, write_offset, entry);

    if (status != WriteStatus::Ok)
    {
        return status;
    }

    set_cached_next_write_offset(valid_page, write_offset + SERIALIZED_ENTRY_SIZE);
    _cache[entry.address] = entry.value;

    return WriteStatus::Ok;
}

WriteStatus EmuEepromInternal::write_entry(Page page, uint32_t offset, Entry entry)
{
    if ((offset + SERIALIZED_ENTRY_SIZE) > _page_size)
    {
        return WriteStatus::PageFull;
    }

    const uint32_t block_offset = (offset / _write_block_size) * _write_block_size;
    const uint32_t within_block = offset - block_offset;

    // Entries are appended in serialized-entry steps and must fit inside one backend write block.
    if ((within_block + SERIALIZED_ENTRY_SIZE) > _write_block_size)
    {
        return WriteStatus::WriteError;
    }

    // Starting a different block means the pending one must be written first.
    if (_write_block_dirty &&
        ((_write_block_page != page) || (_write_block_offset != block_offset)))
    {
        if (!flush_write_block())
        {
            return WriteStatus::WriteError;
        }
    }

    // A fresh staged block starts erased so unused entry slots stay erased when the block is flushed.
    if (!_write_block_dirty)
    {
        std::fill(_write_block_buffer.begin(), _write_block_buffer.end(), ERASED_BYTE);
        _write_block_page    = page;
        _write_block_offset  = block_offset;
        _write_block_entries = 0;
        _write_block_dirty   = true;
    }

    serialize_entry(entry, std::span<uint8_t>(_write_block_buffer.data() + within_block, SERIALIZED_ENTRY_SIZE));
    _write_block_entries++;

    // Full backend blocks can be written immediately; partial blocks wait for flush().
    if (_write_block_entries >= _entries_per_block)
    {
        if (!flush_write_block())
        {
            return WriteStatus::WriteError;
        }
    }

    return WriteStatus::Ok;
}

std::optional<uint32_t> EmuEepromInternal::find_next_free_offset(Page page)
{
    std::array<uint8_t, INTERNAL_BLOCK_SIZE> buffer = {};

    for (uint32_t read_offset = 0;
         read_offset < _page_size;
         read_offset += _read_block_size)
    {
        std::fill(buffer.begin(), buffer.end(), ERASED_BYTE);
        auto read_block = std::span<uint8_t>(buffer.data(), _read_block_size);

        if (!_hwa.read(page, read_offset, read_block))
        {
            return {};
        }

        for (uint32_t within_read_block = 0; within_read_block < _read_block_size; within_read_block += _write_block_size)
        {
            const auto write_block = std::span<const uint8_t>(buffer.data() + within_read_block, _write_block_size);

            if (all_erased(write_block))
            {
                return read_offset + within_read_block;
            }
        }
    }

    return {};
}

WriteStatus EmuEepromInternal::page_transfer()
{
    if (!flush_write_block())
    {
        return WriteStatus::WriteError;
    }

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

    if (!write_page_status(new_page, PageStatus::Receiving))
    {
        return WriteStatus::WriteError;
    }

    set_cached_next_write_offset(new_page, 0);

    /*
     * Because the latest value for every logical address is already mirrored in
     * RAM, page transfer becomes a straight cache dump into the new page.
     */
    for (size_t address = 0; address < _max_address; address++)
    {
        if (_cache[address] == ERASED_VALUE)
        {
            continue;
        }

        const auto status = write_internal(make_entry(static_cast<uint16_t>(address), _cache[address]));

        if (status != WriteStatus::Ok)
        {
            return status;
        }
    }

    if (!flush_write_block())
    {
        return WriteStatus::WriteError;
    }

    if (!_hwa.erase_page(old_page))
    {
        return WriteStatus::WriteError;
    }

    if (!write_page_status(old_page, PageStatus::Formatted))
    {
        return WriteStatus::WriteError;
    }

    if (!write_page_status(new_page, PageStatus::Valid))
    {
        return WriteStatus::WriteError;
    }

    if (!cache())
    {
        return WriteStatus::WriteError;
    }

    _cache_dirty = false;
    return WriteStatus::Ok;
}

PageStatus EmuEepromInternal::page_status(Page page)
{
    if (!refresh_page_status(page))
    {
        return PageStatus::Erased;
    }

    return cached_page_status(page);
}

bool EmuEepromInternal::refresh_page_status(Page page)
{
    uint32_t offset     = _page_size - SERIALIZED_ENTRY_SIZE;
    Page     write_page = Page::Page1;

    if (find_valid_page(PageOperation::Write, write_page) && (write_page == page))
    {
        auto cached_write_offset = cached_next_write_offset();

        if (((cached_write_offset.page != page) || (cached_write_offset.offset == 0)) &&
            !refresh_next_write_offset(page))
        {
            return false;
        }

        cached_write_offset = cached_next_write_offset();

        if (cached_write_offset.offset > 0)
        {
            offset = std::min(cached_write_offset.offset, static_cast<uint32_t>(_page_size)) - SERIALIZED_ENTRY_SIZE;
        }
    }

    std::array<uint8_t, INTERNAL_BLOCK_SIZE> buffer = {};

    while (true)
    {
        const uint32_t read_block_offset = (offset / _read_block_size) * _read_block_size;

        std::fill(buffer.begin(), buffer.end(), ERASED_BYTE);

        if (!_hwa.read(page, read_block_offset, std::span<uint8_t>(buffer.data(), _read_block_size)))
        {
            return false;
        }

        uint32_t within_read_block = offset - read_block_offset;

        while (true)
        {
            const auto entry_data = std::span<const uint8_t>(buffer.data() + within_read_block, SERIALIZED_ENTRY_SIZE);
            const auto entry      = deserialize_entry(entry_data);

            if (entry.address == RESERVED_STATUS_ADDRESS)
            {
                const auto status = deserialize_page_status(entry.value);

                if (status.has_value())
                {
                    set_cached_page_status(page, status.value());
                    return true;
                }
            }

            if (within_read_block < SERIALIZED_ENTRY_SIZE)
            {
                break;
            }

            within_read_block -= SERIALIZED_ENTRY_SIZE;
        }

        if (read_block_offset == 0)
        {
            break;
        }

        offset = read_block_offset - SERIALIZED_ENTRY_SIZE;
    }

    set_cached_page_status(page, PageStatus::Erased);
    return true;
}

PageStatus EmuEepromInternal::cached_page_status(Page page) const
{
    return _page_status_cache[static_cast<size_t>(page)];
}

void EmuEepromInternal::set_cached_page_status(Page page, PageStatus status)
{
    _page_status_cache[static_cast<size_t>(page)] = status;
}

bool EmuEepromInternal::refresh_next_write_offset(Page page)
{
    std::array<uint8_t, INTERNAL_BLOCK_SIZE> buffer = {};

    for (uint32_t read_offset = 0;
         read_offset < _page_size;
         read_offset += _read_block_size)
    {
        std::fill(buffer.begin(), buffer.end(), ERASED_BYTE);
        auto read_block = std::span<uint8_t>(buffer.data(), _read_block_size);

        if (!_hwa.read(page, read_offset, read_block))
        {
            return false;
        }

        for (uint32_t within_read_block = 0; within_read_block < _read_block_size; within_read_block += _write_block_size)
        {
            const auto write_block = std::span<const uint8_t>(buffer.data() + within_read_block, _write_block_size);

            if (all_erased(write_block))
            {
                set_cached_next_write_offset(page, read_offset + within_read_block);
                return true;
            }
        }
    }

    set_cached_next_write_offset(page, _page_size);
    return true;
}

EmuEepromInternal::NextWriteOffsetCache EmuEepromInternal::cached_next_write_offset() const
{
    return _next_write_offset_cache;
}

void EmuEepromInternal::set_cached_next_write_offset(Page page, uint32_t offset)
{
    _next_write_offset_cache = NextWriteOffsetCache{
        .page   = page,
        .offset = offset,
    };
}

bool EmuEepromInternal::write_page_status(Page page, PageStatus status)
{
    if (!flush_write_block())
    {
        return false;
    }

    const auto offset = find_next_free_offset(page);

    if (!offset.has_value())
    {
        return false;
    }

    const auto write_status = write_entry(page, offset.value(), make_entry(RESERVED_STATUS_ADDRESS, serialize_page_status(status)));

    if (write_status != WriteStatus::Ok)
    {
        return false;
    }

    if (!flush_write_block())
    {
        return false;
    }

    const auto cached_write_offset = cached_next_write_offset();

    if (cached_write_offset.page == page)
    {
        const uint32_t next_offset = std::min(static_cast<uint32_t>(((offset.value() / _write_block_size) * _write_block_size) + _write_block_size),
                                              static_cast<uint32_t>(_page_size));

        set_cached_next_write_offset(page, next_offset);
    }

    set_cached_page_status(page, status);
    return true;
}

EmuEepromInternal::EntryValue EmuEepromInternal::serialize_page_status(PageStatus status)
{
    return static_cast<EntryValue>(status);
}

std::optional<PageStatus> EmuEepromInternal::deserialize_page_status(EntryValue value)
{
    switch (value)
    {
    case static_cast<EntryValue>(PageStatus::Valid):
        return PageStatus::Valid;

    case static_cast<EntryValue>(PageStatus::Formatted):
        return PageStatus::Formatted;

    case static_cast<EntryValue>(PageStatus::Receiving):
        return PageStatus::Receiving;

    default:
        return {};
    }
}

std::optional<Entry> EmuEepromInternal::read_entry(Page page, uint32_t offset)
{
    const uint32_t block_offset = (offset / _write_block_size) * _write_block_size;
    const uint32_t within_block = offset - block_offset;

    if ((within_block + SERIALIZED_ENTRY_SIZE) > _write_block_size)
    {
        return {};
    }

    std::array<uint8_t, INTERNAL_BLOCK_SIZE> buffer = {};
    std::fill(buffer.begin(), buffer.end(), ERASED_BYTE);

    if (!_hwa.read(page, block_offset, std::span<uint8_t>(buffer.data(), _write_block_size)))
    {
        return {};
    }

    return deserialize_entry(std::span<const uint8_t>(buffer.data() + within_block, SERIALIZED_ENTRY_SIZE));
}

bool EmuEepromInternal::flush_write_block()
{
    if (!_write_block_dirty)
    {
        return true;
    }

    if (!_hwa.write(_write_block_page,
                    _write_block_offset,
                    std::span<const uint8_t>(_write_block_buffer.data(), _write_block_size)))
    {
        return false;
    }

    const auto cached_write_offset = cached_next_write_offset();

    if ((cached_write_offset.page == _write_block_page) &&
        (cached_write_offset.offset > _write_block_offset) &&
        (cached_write_offset.offset < (_write_block_offset + _write_block_size)))
    {
        set_cached_next_write_offset(_write_block_page, _write_block_offset + _write_block_size);
    }

    clear_write_block();
    return true;
}

void EmuEepromInternal::clear_write_block()
{
    std::fill(_write_block_buffer.begin(), _write_block_buffer.end(), ERASED_BYTE);
    _write_block_page    = Page::Page1;
    _write_block_offset  = 0;
    _write_block_entries = 0;
    _write_block_dirty   = false;
}

bool EmuEepromInternal::cache()
{
    Page valid_page;

    clear_write_block();
    std::fill(_cache.begin(), _cache.end(), ERASED_VALUE);
    set_cached_next_write_offset(Page::Page1, 0);
    _cache_dirty = false;

    if (!find_valid_page(PageOperation::Read, valid_page))
    {
        return false;
    }

    std::array<uint8_t, INTERNAL_BLOCK_SIZE> buffer = {};

    for (uint32_t read_offset = 0;
         read_offset < _page_size;
         read_offset += _read_block_size)
    {
        std::fill(buffer.begin(), buffer.end(), ERASED_BYTE);
        auto read_block = std::span<uint8_t>(buffer.data(), _read_block_size);

        if (!_hwa.read(valid_page, read_offset, read_block))
        {
            return false;
        }

        for (uint32_t within_read_block = 0; within_read_block < _read_block_size; within_read_block += _write_block_size)
        {
            const auto write_block = std::span<const uint8_t>(buffer.data() + within_read_block, _write_block_size);

            if (all_erased(write_block))
            {
                set_cached_next_write_offset(valid_page, read_offset + within_read_block);
                clear_write_block();
                return true;
            }

            set_cached_next_write_offset(valid_page, read_offset + within_read_block + _write_block_size);

            for (size_t entry = 0; entry < _entries_per_block; entry++)
            {
                const size_t within_write_block = entry * SERIALIZED_ENTRY_SIZE;
                const auto   entry_data         = std::span<const uint8_t>(buffer.data() + within_read_block + within_write_block,
                                                                           SERIALIZED_ENTRY_SIZE);

                if (all_erased(entry_data))
                {
                    continue;
                }

                const auto decoded = deserialize_entry(entry_data);

                if (decoded.address == RESERVED_STATUS_ADDRESS)
                {
                    continue;
                }

                if (decoded.address >= max_address())
                {
                    return false;
                }

                _cache[decoded.address] = decoded.value;
            }
        }
    }

    clear_write_block();
    return true;
}

bool EmuEepromInternal::flush()
{
    if (_cache_dirty)
    {
        if (page_transfer() != WriteStatus::Ok)
        {
            return false;
        }
    }

    return flush_write_block();
}
