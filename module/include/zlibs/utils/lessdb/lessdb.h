/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lessdb_deps.h"

#include "zlibs/utils/misc/mutex.h"

namespace zlibs::utils::lessdb
{
    /**
     * @brief Lightweight parameter database backed by an arbitrary storage medium.
     *
     * All public methods are thread-safe.
     */
    class LessDb
    {
        public:
        /**
         * @brief Constructs a database instance bound to a hardware abstraction object.
         *
         * @param hwa Hardware abstraction providing storage read/write access.
         */
        explicit LessDb(Hwa& hwa)
            : _hwa(hwa)
        {}

        /**
         * @brief Initializes the underlying storage hardware.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init();

        /**
         * @brief Assigns a layout and validates address-space fit.
         *
         * Section offsets from block start must already be embedded in the
         * Section objects by make_block(). Block offsets from layout start
         * must already be embedded in the Block objects by make_layout(). This function stores
         * start_address and validates that the layout fits in hardware address
         * space starting from that address.
         *
         * Layout descriptors are intended to be created at compile time by
         * make_block() and make_layout().
         *
         * @param layout        View over the block-and-section layout descriptor.
         *                      The referenced data must remain valid for the lifetime
         *                      of this LessDb instance (or until the next set_layout call).
         * @param start_address Address offset at which the first block begins.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool set_layout(std::span<const Block> layout, uint32_t start_address = 0);

        /**
         * @brief Computes a layout UID at compile time for fixed-size layout arrays.
         *
         * This overload enables constexpr evaluation when `layout` is a
         * compile-time `std::array<Block, N>`.
         *
         * @param layout Compile-time layout descriptor.
         *
         * @return 32-bit layout UID, or zero for an empty array.
         */
        template<size_t N>
        static constexpr uint32_t layout_uid(const std::array<Block, N>& layout)
        {
            if constexpr (N == 0)
            {
                return 0;
            }

            constexpr uint32_t FNV1A32_OFFSET_BASIS = 2166136261u;
            constexpr uint32_t FNV1A32_PRIME        = 16777619u;

            uint32_t hash = FNV1A32_OFFSET_BASIS;

            auto mix = [&hash](uint32_t value)
            {
                hash ^= value;
                hash *= FNV1A32_PRIME;
            };

            mix(static_cast<uint32_t>(N));

            for (size_t block = 0; block < N; block++)
            {
                mix(static_cast<uint32_t>(layout[block]._sections.size()));

                for (size_t section = 0; section < layout[block]._sections.size(); section++)
                {
                    mix(static_cast<uint32_t>(layout[block]._sections[section].NUMBER_OF_PARAMETERS));
                    mix(static_cast<uint32_t>(layout[block]._sections[section].PARAMETER_TYPE));
                }
            }

            return hash;
        }

        /**
         * @brief Clears the entire storage medium.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool clear();

        /**
         * @brief Reads a parameter value from the database.
         *
         * @param block_index     Block index.
         * @param section_index   Section index within the block.
         * @param parameter_index Parameter index within the section.
         *
         * @return Parameter value on success, otherwise `std::nullopt`.
         */
        std::optional<uint32_t> read(size_t block_index, size_t section_index, size_t parameter_index);

        /**
         * @brief Updates a parameter value in the database.
         *
         * @param block_index     Block index.
         * @param section_index   Section index within the block.
         * @param parameter_index Parameter index within the section.
         * @param new_value       Value to write.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool update(size_t block_index, size_t section_index, size_t parameter_index, uint32_t new_value);

        /**
         * @brief Returns the number of bytes used by a layout descriptor.
         *
         * This helper is constexpr-friendly, so it can be evaluated for
         * compile-time layouts produced by make_layout().
         *
         * @param layout Layout descriptor to inspect.
         *
         * @return Memory usage in bytes.
         */
        static constexpr uint32_t layout_size(std::span<const Block> layout)
        {
            if (layout.empty())
            {
                return 0;
            }

            return layout[layout.size() - 1].BLOCK_OFFSET + layout[layout.size() - 1].byte_size();
        }

        /**
         * @brief Returns the total storage capacity of the underlying medium.
         *
         * @return Maximum database size in bytes.
         */
        uint32_t db_size() const;

        /**
         * @brief Writes default values to all sections in the active layout.
         *
         * @param type `FactoryResetType::Full` overwrites all data;
         *             `FactoryResetType::Partial` skips sections marked with
         *             `PreserveSetting::Enable`.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init_data(FactoryResetType type = FactoryResetType::Full);

        private:
        static constexpr uint32_t INVALID_ADDRESS      = 0xFFFFFFFF;
        static constexpr uint32_t HALF_BYTE_MASK_LOWER = 0x0F;
        static constexpr uint32_t HALF_BYTE_MASK_UPPER = 0xF0;
        static constexpr uint32_t BYTE_MASK            = 0xFF;
        static constexpr uint32_t WORD_MASK            = 0xFFFF;

        static constexpr uint8_t BIT_MASK[8] = {
            0b00000001,
            0b00000010,
            0b00000100,
            0b00001000,
            0b00010000,
            0b00100000,
            0b01000000,
            0b10000000,
        };

        Hwa&                              _hwa;
        uint32_t                          _initial_address   = 0;
        uint8_t                           _last_read_value   = 0;
        uint32_t                          _last_read_address = INVALID_ADDRESS;
        std::span<const Block>            _layout;
        mutable zlibs::utils::misc::Mutex _mutex;

        /**
         * @brief Writes a raw value to storage and verifies it by reading it back.
         *
         * @param address Absolute storage address.
         * @param value   Value to write.
         * @param type    Parameter type determining the storage width.
         *
         * @return `true` when the write succeeds and the read-back value matches, otherwise `false`.
         */
        bool write(uint32_t address, uint32_t value, SectionParameterType type);

        /**
         * @brief Verifies that a block/section/parameter tuple is within the active layout.
         *
         * @param block_index     Block index.
         * @param section_index   Section index within the block.
         * @param parameter_index Parameter index within the section.
         *
         * @return `true` when all indices are valid, otherwise `false`.
         */
        bool check_parameters(size_t block_index, size_t section_index, size_t parameter_index);

        /**
         * @brief Computes the absolute storage address of a section.
         *
         * @param block_index   Block index.
         * @param section_index Section index within the block.
         *
         * @return Absolute storage address of the section start.
         */
        uint32_t section_address(size_t block_index, size_t section_index);
    };
}    // namespace zlibs::utils::lessdb
