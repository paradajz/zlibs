/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lessdb_deps.h"

#include "zlibs/utils/misc/bit.h"
#include "zlibs/utils/misc/mutex.h"

#include <functional>
#include <vector>

namespace zlibs::utils::lessdb
{
    /**
     * @brief Describes one value requested during database initialization.
     */
    struct InitRequest
    {
        size_t block_index     = 0;
        size_t section_index   = 0;
        size_t parameter_index = 0;
    };

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

        using InitProvider = std::function<std::optional<uint32_t>(const InitRequest& request)>;

        /**
         * @brief Initializes the underlying storage hardware.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool init();

        /**
         * @brief Assigns a compile-time layout and validates address-space fit.
         *
         * Section offsets from block start must already be embedded in the
         * Section objects by make_block(). Block offsets from layout start
         * must already be embedded in the Block objects by make_layout().
         *
         * @param layout        Compile-time layout descriptor.
         * @param start_address Address offset at which the first block begins.
         *
         * @return `true` on success, otherwise `false`.
         */
        template<size_t N>
        bool set_layout(const std::array<Block, N>& layout, uint32_t start_address = 0)
        {
            return set_layout(layout, start_address, layout_uid(layout));
        }

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
                    mix(static_cast<uint32_t>(layout[block]._sections[section]._number_of_parameters));
                    mix(static_cast<uint32_t>(layout[block]._sections[section]._parameter_type));
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
         * @brief Registers an initializer for values in one layout section.
         *
         * The provider is called by init_data() before writing each matching
         * parameter. Returning a value overrides the section's layout default;
         * returning std::nullopt keeps the layout default.
         *
         * Providers are scoped to the active layout UID. Use the array
         * set_layout() overload to bind providers to a compile-time layout UID.
         *
         * Providers run while LessDb is initializing data and must not call
         * back into the same LessDb instance.
         *
         * @param block_index   Block index within the layout.
         * @param section_index Section index within the block.
         * @param provider      Initial value provider.
         */
        void register_layout_init_provider(size_t block_index, size_t section_index, InitProvider&& provider);

        /**
         * @brief Removes all registered initialization providers.
         */
        void clear_init_providers();

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

            return layout[layout.size() - 1]._block_offset + layout[layout.size() - 1].byte_size();
        }

        /**
         * @brief Returns the logical address capacity of the underlying medium.
         *
         * @return Number of logical addresses available to LessDB.
         */
        uint32_t address_count() const;

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
        static constexpr uint32_t INVALID_ADDRESS = 0xFFFFFFFF;

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
        uint32_t                          _layout_uid        = 0;
        uint8_t                           _last_read_value   = 0;
        uint32_t                          _last_read_address = INVALID_ADDRESS;
        std::span<const Block>            _layout;
        mutable zlibs::utils::misc::Mutex _mutex;

        struct InitProviderEntry
        {
            uint32_t     layout_uid;
            size_t       block_index;
            size_t       section_index;
            InitProvider provider;
        };

        std::vector<InitProviderEntry> _init_providers;

        /**
         * @brief Assigns a layout without a compile-time layout UID.
         *
         * This compatibility path scopes init providers to UID 0.
         *
         * @param layout        Layout descriptor.
         * @param start_address Address offset at which the first block begins.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool set_layout(std::span<const Block> layout, uint32_t start_address);

        /**
         * @brief Assigns a layout with a precomputed layout UID.
         *
         * @param layout        Layout descriptor.
         * @param start_address Address offset at which the first block begins.
         * @param layout_uid    UID associated with the layout descriptor.
         *
         * @return `true` on success, otherwise `false`.
         */
        bool set_layout(std::span<const Block> layout, uint32_t start_address, uint32_t layout_uid);

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
         * @brief Returns an initialized value for a parameter.
         *
         * Registered init providers may override the provided layout default.
         *
         * @param block_index     Block index.
         * @param section_index   Section index within the block.
         * @param parameter_index Parameter index within the section.
         * @param default_value   Default value declared by the active layout.
         *
         * @return Provider-supplied value, or default_value when no provider overrides it.
         */
        uint32_t init_value(size_t block_index, size_t section_index, size_t parameter_index, uint32_t default_value) const;

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
