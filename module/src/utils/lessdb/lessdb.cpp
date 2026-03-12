/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/lessdb/lessdb.h"

using namespace zlibs::utils::lessdb;
using namespace zlibs::utils::misc;

bool LessDb::init()
{
    return _hwa.init();
}

bool LessDb::set_layout(std::span<const Block> layout, uint32_t start_address)
{
    const LockGuard LOCK(_mutex);

    if (start_address >= _hwa.size())
    {
        return false;
    }

    if (layout.empty())
    {
        return false;
    }

    uint32_t total_usage = layout_size(layout);
    uint32_t available   = _hwa.size() - start_address;

    if (total_usage > available)
    {
        return false;
    }

    _initial_address = start_address;
    _layout          = layout;

    return true;
}

std::optional<uint32_t> LessDb::read(size_t block_index, size_t section_index, size_t parameter_index)
{
    const LockGuard LOCK(_mutex);
    uint32_t        value = 0;

    if (!check_parameters(block_index, section_index, parameter_index))
    {
        return {};
    }

    bool    ret           = true;
    auto    start_address = section_address(block_index, section_index);
    uint8_t array_index   = 0;

    switch (_layout[block_index]._sections[section_index].PARAMETER_TYPE)
    {
    case SectionParameterType::Bit:
    {
        array_index = parameter_index / BIT_VALUES_IN_BYTE;
        start_address += array_index;
        const uint8_t BIT_INDEX = static_cast<uint8_t>(parameter_index - (array_index * BIT_VALUES_IN_BYTE));

        if (start_address == _last_read_address)
        {
            value = static_cast<bool>(_last_read_value & BIT_MASK[BIT_INDEX]);
        }
        else if (const auto READ_VALUE = _hwa.read(start_address, SectionParameterType::Bit); READ_VALUE.has_value())
        {
            _last_read_value = READ_VALUE.value();
            value            = static_cast<bool>(READ_VALUE.value() & BIT_MASK[BIT_INDEX]);
        }
        else
        {
            ret = false;
        }
    }
    break;

    case SectionParameterType::Byte:
    {
        start_address += parameter_index;

        if (const auto READ_VALUE = _hwa.read(start_address, SectionParameterType::Byte); READ_VALUE.has_value())
        {
            value = READ_VALUE.value() & static_cast<int32_t>(BYTE_MASK);
        }
        else
        {
            ret = false;
        }
    }
    break;

    case SectionParameterType::HalfByte:
    {
        start_address += parameter_index / HALF_BYTE_VALUES_IN_BYTE;

        if (start_address == _last_read_address)
        {
            value = _last_read_value;

            if (parameter_index % HALF_BYTE_VALUES_IN_BYTE)
            {
                value >>= 4;
            }
        }
        else if (const auto READ_VALUE = _hwa.read(start_address, SectionParameterType::HalfByte); READ_VALUE.has_value())
        {
            _last_read_value = READ_VALUE.value();
            value            = READ_VALUE.value();

            if (parameter_index % HALF_BYTE_VALUES_IN_BYTE)
            {
                value >>= 4;
            }
        }
        else
        {
            ret = false;
        }

        if (ret)
        {
            value &= HALF_BYTE_MASK_LOWER;
        }
    }
    break;

    case SectionParameterType::Word:
    {
        start_address += parameter_index * WORD_SIZE_IN_BYTES;

        if (const auto READ_VALUE = _hwa.read(start_address, SectionParameterType::Word); READ_VALUE.has_value())
        {
            value = READ_VALUE.value() & WORD_MASK;
        }
        else
        {
            ret = false;
        }
    }
    break;

    default:
    {
        // case SectionParameterType::Dword:
        start_address += parameter_index * DWORD_SIZE_IN_BYTES;
        const auto READ_VALUE = _hwa.read(start_address, SectionParameterType::Dword);

        if (READ_VALUE.has_value())
        {
            value = READ_VALUE.value();
            return value;
        }

        return {};
    }
    break;
    }

    if (ret)
    {
        _last_read_address = start_address;
    }

    if (!ret)
    {
        return {};
    }

    return value;
}

bool LessDb::update(size_t block_index, size_t section_index, size_t parameter_index, uint32_t new_value)
{
    const LockGuard LOCK(_mutex);

    if (_layout.empty())
    {
        return false;
    }

    if (!check_parameters(block_index, section_index, parameter_index))
    {
        return false;
    }

    auto                 start_address  = section_address(block_index, section_index);
    SectionParameterType parameter_type = _layout[block_index]._sections[section_index].PARAMETER_TYPE;
    uint8_t              array_index    = 0;
    uint32_t             array_value    = 0;
    uint8_t              bit_index      = 0;

    switch (parameter_type)
    {
    case SectionParameterType::Bit:
    {
        new_value &= static_cast<uint32_t>(0x01);
        array_index = parameter_index / BIT_VALUES_IN_BYTE;
        bit_index   = parameter_index - BIT_VALUES_IN_BYTE * array_index;
        start_address += array_index;

        if (const auto READ_VALUE = _hwa.read(start_address, SectionParameterType::Bit); READ_VALUE.has_value())
        {
            array_value = READ_VALUE.value();

            if (new_value)
            {
                array_value |= BIT_MASK[bit_index];
            }
            else
            {
                array_value &= ~BIT_MASK[bit_index];
            }

            return write(start_address, array_value, SectionParameterType::Bit);
        }
    }
    break;

    case SectionParameterType::Byte:
    {
        new_value &= BYTE_MASK;
        start_address += parameter_index;
        return write(start_address, new_value, SectionParameterType::Byte);
    }
    break;

    case SectionParameterType::HalfByte:
    {
        new_value &= HALF_BYTE_MASK_LOWER;
        start_address += (parameter_index / HALF_BYTE_VALUES_IN_BYTE);

        if (const auto READ_VALUE = _hwa.read(start_address, SectionParameterType::HalfByte); READ_VALUE.has_value())
        {
            array_value = READ_VALUE.value();

            if (parameter_index % HALF_BYTE_VALUES_IN_BYTE)
            {
                array_value &= HALF_BYTE_MASK_LOWER;
                array_value |= (new_value << 4);
            }
            else
            {
                array_value &= HALF_BYTE_MASK_UPPER;
                array_value |= new_value;
            }

            return write(start_address, array_value, SectionParameterType::HalfByte);
        }
    }
    break;

    case SectionParameterType::Word:
    {
        new_value &= WORD_MASK;
        start_address += (parameter_index * WORD_SIZE_IN_BYTES);
        return write(start_address, new_value, SectionParameterType::Word);
    }
    break;

    case SectionParameterType::Dword:
    {
        start_address += (parameter_index * DWORD_SIZE_IN_BYTES);
        return write(start_address, new_value, SectionParameterType::Dword);
    }
    break;
    }

    return false;
}

bool LessDb::write(uint32_t address, uint32_t value, SectionParameterType type)
{
    if (address == _last_read_address)
    {
        _last_read_address = INVALID_ADDRESS;
    }

    if (_hwa.write(address, value, type))
    {
        const auto READ_VALUE = _hwa.read(address, type);

        if (READ_VALUE.has_value())
        {
            return (value == READ_VALUE.value());
        }
    }

    return false;
}

bool LessDb::clear()
{
    const LockGuard LOCK(_mutex);
    return _hwa.clear();
}

bool LessDb::init_data(FactoryResetType type)
{
    const LockGuard LOCK(_mutex);

    for (size_t block = 0; block < _layout.size(); block++)
    {
        for (size_t section = 0; section < _layout[block]._sections.size(); section++)
        {
            if (
                (_layout[block]._sections[section].PRESERVE_ON_PARTIAL_RESET == PreserveSetting::Enable) &&
                (type == FactoryResetType::Partial))
            {
                continue;
            }

            auto start_address        = section_address(block, section);
            auto parameter_type       = _layout[block]._sections[section].PARAMETER_TYPE;
            auto default_value        = _layout[block]._sections[section].DEFAULT_VALUE;
            auto default_values       = _layout[block]._sections[section].DEFAULT_VALUES;
            auto number_of_parameters = _layout[block]._sections[section].NUMBER_OF_PARAMETERS;

            switch (parameter_type)
            {
            case SectionParameterType::Byte:
            case SectionParameterType::Word:
            case SectionParameterType::Dword:
            {
                for (size_t parameter = 0; parameter < number_of_parameters; parameter++)
                {
                    if (_layout[block]._sections[section].AUTO_INCREMENT == AutoIncrementSetting::Enable)
                    {
                        if (!write(start_address, default_value + parameter, parameter_type))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (default_values.size() == number_of_parameters)
                        {
                            default_value = default_values[parameter];
                        }

                        if (!write(start_address, default_value, parameter_type))
                        {
                            return false;
                        }
                    }

                    if (parameter_type == SectionParameterType::Byte)
                    {
                        start_address++;
                    }
                    else if (parameter_type == SectionParameterType::Word)
                    {
                        start_address += WORD_SIZE_IN_BYTES;
                    }
                    else if (parameter_type == SectionParameterType::Dword)
                    {
                        start_address += DWORD_SIZE_IN_BYTES;
                    }
                }
            }
            break;

            case SectionParameterType::Bit:
            {
                size_t loops = (number_of_parameters / BIT_VALUES_IN_BYTE) + ((number_of_parameters % BIT_VALUES_IN_BYTE) != 0);

                for (size_t loop = 0; loop < loops; loop++)
                {
                    uint8_t value = 0;

                    for (uint8_t bit = 0; bit < BIT_VALUES_IN_BYTE; bit++)
                    {
                        const size_t PARAMETER = (loop * BIT_VALUES_IN_BYTE) + bit;

                        if (PARAMETER >= number_of_parameters)
                        {
                            break;
                        }

                        if (default_values.size() == number_of_parameters)
                        {
                            default_value = default_values[PARAMETER] & 0x01;
                        }

                        value <<= 1;
                        value |= default_value;
                    }

                    if (!write(start_address, value, SectionParameterType::Byte))
                    {
                        return false;
                    }

                    start_address++;
                }
            }
            break;

            case SectionParameterType::HalfByte:
            {
                size_t loops = (number_of_parameters / HALF_BYTE_VALUES_IN_BYTE) + ((number_of_parameters % HALF_BYTE_VALUES_IN_BYTE) != 0);

                for (size_t loop = 0; loop < loops; loop++)
                {
                    uint8_t value = 0;

                    for (uint8_t half_byte = 0; half_byte < HALF_BYTE_VALUES_IN_BYTE; half_byte++)
                    {
                        const size_t PARAMETER = (loop * HALF_BYTE_VALUES_IN_BYTE) + half_byte;

                        if (PARAMETER >= number_of_parameters)
                        {
                            break;
                        }

                        if (default_values.size() == number_of_parameters)
                        {
                            default_value = default_values[PARAMETER] & HALF_BYTE_MASK_LOWER;
                        }

                        value <<= 4;
                        value |= default_value;
                    }

                    if (!write(start_address, value, SectionParameterType::Byte))
                    {
                        return false;
                    }

                    start_address++;
                }
            }
            break;
            }
        }
    }

    return true;
}

uint32_t LessDb::db_size() const
{
    return _hwa.size();
}

bool LessDb::check_parameters(size_t block_index, size_t section_index, size_t parameter_index)
{
    if (block_index >= _layout.size())
    {
        return false;
    }

    if (section_index >= _layout[block_index]._sections.size())
    {
        return false;
    }

    if (parameter_index >= _layout[block_index]._sections[section_index].NUMBER_OF_PARAMETERS)
    {
        return false;
    }

    return true;
}

uint32_t LessDb::section_address(size_t block_index, size_t section_index)
{
    return _initial_address + _layout[block_index].BLOCK_OFFSET + _layout[block_index]._sections[section_index].ADDRESS;
}
