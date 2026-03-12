/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace zlibs::utils::emueeprom
{
    /**
     * @brief Flash-page header state used by the emulated EEPROM state machine.
     */
    enum class PageStatus : uint32_t
    {
        Valid     = 0x00000000,
        Erased    = 0xFFFFFFFF,
        Formatted = 0xFFFFEEEE,
        Receiving = 0xEEEEEEEE,
    };

    /**
     * @brief Result of a logical EEPROM read.
     */
    enum class ReadStatus : uint8_t
    {
        Ok,
        NoVariable,
        NoPage,
        ReadError,
    };

    /**
     * @brief Result of a logical EEPROM write or page transfer.
     */
    enum class WriteStatus : uint8_t
    {
        Ok,
        PageFull,
        NoPage,
        WriteError,
    };

    /**
     * @brief Flash page roles used by the emulation algorithm.
     */
    enum class Page : uint8_t
    {
        Page1,
        Page2,
        Factory,
    };
}    // namespace zlibs::utils::emueeprom
