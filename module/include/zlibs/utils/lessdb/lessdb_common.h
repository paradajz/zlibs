/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace zlibs::utils::lessdb
{
    class LessDb;
    class Block;

    /** @brief Number of bit-type parameters packed into one byte. */
    constexpr inline uint32_t BIT_VALUES_IN_BYTE = 8;

    /** @brief Number of half-byte-type parameters packed into one byte. */
    constexpr inline uint32_t HALF_BYTE_VALUES_IN_BYTE = 2;

    /** @brief Number of bytes occupied by one word-type parameter. */
    constexpr inline uint32_t WORD_SIZE_IN_BYTES = 2;

    /** @brief Number of bytes occupied by one dword-type parameter. */
    constexpr inline uint32_t DWORD_SIZE_IN_BYTES = 4;

    /**
     * @brief Storage type for a section parameter.
     */
    enum class SectionParameterType : uint8_t
    {
        Bit,
        Byte,
        HalfByte,
        Word,
        Dword,
    };

    /**
     * @brief Controls whether a full or partial factory reset is applied.
     */
    enum class FactoryResetType : uint8_t
    {
        Partial,
        Full,
    };

    /**
     * @brief Controls whether a section is preserved during a partial reset.
     */
    enum class PreserveSetting : uint8_t
    {
        Enable,
        Disable,
    };

    /**
     * @brief Controls whether section default values auto-increment per parameter index.
     */
    enum class AutoIncrementSetting : uint8_t
    {
        Enable,
        Disable,
    };

    /**
     * @brief Returns the number of bytes occupied by `n` parameters of a given type.
     *
     * @param n Number of parameters.
     * @param t Storage type of each parameter.
     *
     * @return Byte count required to store `n` parameters of type `t`.
     */
    constexpr uint32_t section_byte_size(size_t n, SectionParameterType t)
    {
        switch (t)
        {
        case SectionParameterType::Bit:
            return static_cast<uint32_t>((n / BIT_VALUES_IN_BYTE) + (n % BIT_VALUES_IN_BYTE != 0));

        case SectionParameterType::HalfByte:
            return static_cast<uint32_t>((n / HALF_BYTE_VALUES_IN_BYTE) + (n % HALF_BYTE_VALUES_IN_BYTE != 0));

        case SectionParameterType::Byte:
            return static_cast<uint32_t>(n);

        case SectionParameterType::Word:
            return static_cast<uint32_t>(n * WORD_SIZE_IN_BYTES);

        default:    // Dword
            return static_cast<uint32_t>(n * DWORD_SIZE_IN_BYTES);
        }
    }

    // Forward declarations required before Section definition.
    class Section;

    /**
     * @brief Implementation helpers for make_block() and make_layout(); not part of the public API.
     */
    namespace internal
    {
        /**
         * @brief Pack-expansion helper used by make_block().
         *
         * Builds a new section array by copying each input section and stamping
         * the precomputed per-section address from `addresses`.
         *
         * @param sections  Input sections with ADDRESS == 0.
         * @param addresses Precomputed offset (in bytes) from block start for each section.
         *
         * @return New array of sections with ADDRESS fields set.
         */
        template<size_t N, size_t... I>
        consteval std::array<Section, N> make_block_impl(
            const std::array<Section, N>&  sections,
            const std::array<uint32_t, N>& addresses,
            std::index_sequence<I...>);

        /**
         * @brief Pack-expansion helper used by make_layout().
         *
         * Builds a new block array by copying each input block and stamping
         * the precomputed per-block offset from `offsets`.
         *
         * @param blocks  Input blocks with BLOCK_OFFSET == 0.
         * @param offsets Precomputed offset (in bytes) from layout start for each block.
         *
         * @return New array of blocks with BLOCK_OFFSET fields set.
         */
        template<size_t N, size_t... I>
        consteval std::array<Block, N> make_layout_impl(
            const std::array<Block, N>&    blocks,
            const std::array<uint32_t, N>& offsets,
            std::index_sequence<I...>);
    }    // namespace internal

    /**
     * @brief Builds a section array with compile-time offsets from block start.
     *
     * Sections are passed with ADDRESS == 0; make_block returns a new array
     * where each section carries the correct cumulative byte offset from the
     * start of the block.
     *
     * @param sections Input sections (order determines layout).
     *
     * @return New array of the same sections with ADDRESS fields filled in.
     */
    template<size_t N>
    consteval std::array<Section, N> make_block(const std::array<Section, N>& sections);

    /**
     * @brief Builds a block array with compile-time offsets from layout start.
     *
     * Blocks are passed with BLOCK_OFFSET == 0; make_layout returns a new array
     * where each block carries the correct cumulative byte offset from the start
     * of the layout.
     *
     * @param blocks Input blocks (order determines layout).
     *
     * @return New array of the same blocks with BLOCK_OFFSET fields filled in.
     */
    template<size_t N>
    consteval std::array<Block, N> make_layout(const std::array<Block, N>& blocks);

    /**
     * @brief Describes one section within a database block.
     *
     * All configuration fields are compile-time constants. The section start
     * offset in its block (ADDRESS) is also constant and must be set by make_block() at the
     * call site; constructing a Section directly leaves the offset at zero.
     */
    class Section
    {
        public:
        /**
         * @brief Constructs a section with a scalar default value.
         *
         * @param number_of_parameters      Number of parameters in this section.
         * @param parameter_type            Storage type for each parameter.
         * @param preserve_on_partial_reset Whether to preserve this section during a partial reset.
         * @param auto_increment            Whether to auto-increment the default value per parameter index.
         * @param default_value             Default value applied to all parameters.
         */
        consteval Section(size_t               number_of_parameters,
                          SectionParameterType parameter_type,
                          PreserveSetting      preserve_on_partial_reset,
                          AutoIncrementSetting auto_increment,
                          uint32_t             default_value)
            : NUMBER_OF_PARAMETERS(number_of_parameters)
            , PARAMETER_TYPE(parameter_type)
            , PRESERVE_ON_PARTIAL_RESET(preserve_on_partial_reset)
            , AUTO_INCREMENT(auto_increment)
            , DEFAULT_VALUE(default_value)
            , DEFAULT_VALUES{}
            , ADDRESS(0)
        {}

        /**
         * @brief Constructs a section with a per-parameter default value list.
         *
         * @param number_of_parameters      Number of parameters in this section.
         * @param parameter_type            Storage type for each parameter.
         * @param preserve_on_partial_reset Whether to preserve this section during a partial reset.
         * @param auto_increment            Whether to auto-increment default values.
         * @param default_values            Per-parameter defaults; the pointed-to array must outlive
         *                                  this section (use a static or constexpr array).
         */
        consteval Section(size_t                    number_of_parameters,
                          SectionParameterType      parameter_type,
                          PreserveSetting           preserve_on_partial_reset,
                          AutoIncrementSetting      auto_increment,
                          std::span<const uint32_t> default_values)
            : NUMBER_OF_PARAMETERS(number_of_parameters)
            , PARAMETER_TYPE(parameter_type)
            , PRESERVE_ON_PARTIAL_RESET(preserve_on_partial_reset)
            , AUTO_INCREMENT(auto_increment)
            , DEFAULT_VALUE(0)
            , DEFAULT_VALUES(default_values)
            , ADDRESS(0)
        {}

        /**
         * @brief Returns the number of bytes this section occupies in storage.
         */
        constexpr uint32_t byte_size() const
        {
            return section_byte_size(NUMBER_OF_PARAMETERS, PARAMETER_TYPE);
        }

        private:
        friend class LessDb;

        template<size_t N, size_t... I>
        friend consteval std::array<Section, N> internal::make_block_impl(
            const std::array<Section, N>&,
            const std::array<uint32_t, N>&,
            std::index_sequence<I...>);

        template<size_t N>
        friend consteval std::array<Section, N> make_block(const std::array<Section, N>&);

        // Private copy constructor that stamps in the computed offset from block start.
        // Only callable by make_block (via friend).
        consteval Section(const Section& other, uint32_t address)
            : NUMBER_OF_PARAMETERS(other.NUMBER_OF_PARAMETERS)
            , PARAMETER_TYPE(other.PARAMETER_TYPE)
            , PRESERVE_ON_PARTIAL_RESET(other.PRESERVE_ON_PARTIAL_RESET)
            , AUTO_INCREMENT(other.AUTO_INCREMENT)
            , DEFAULT_VALUE(other.DEFAULT_VALUE)
            , DEFAULT_VALUES(other.DEFAULT_VALUES)
            , ADDRESS(address)
        {}

        const size_t                    NUMBER_OF_PARAMETERS;
        const SectionParameterType      PARAMETER_TYPE;
        const PreserveSetting           PRESERVE_ON_PARTIAL_RESET;
        const AutoIncrementSetting      AUTO_INCREMENT;
        const uint32_t                  DEFAULT_VALUE;
        const std::span<const uint32_t> DEFAULT_VALUES;
        const uint32_t                  ADDRESS;
    };

    /**
     * @brief Describes one block within the database layout.
     *
     * A Block is a non-owning view over a contiguous array of Section objects.
     * The referenced array must outlive the Block and any LessDb that uses it
     * (use static or constexpr storage).
     *
     * The block start offset in the layout (BLOCK_OFFSET) is constant and must be
     * set by make_layout() at the call site; constructing a Block directly
     * leaves the offset at zero.
     */
    class Block
    {
        public:
        /**
         * @brief Constructs a block from a compile-time section array.
         *
         * @param sections Reference to the sections making up this block. The array
         *                 must remain valid for the lifetime of this Block.
         */
        template<size_t N>
        consteval explicit Block(const std::array<Section, N>& sections)
            : _sections(sections)
            , BLOCK_OFFSET(0)
        {}

        /**
         * @brief Returns the total number of bytes this block occupies in storage.
         */
        constexpr uint32_t byte_size() const
        {
            uint32_t size = 0;

            for (const auto& section : _sections)
            {
                size += section.byte_size();
            }

            return size;
        }

        private:
        friend class LessDb;

        template<size_t N, size_t... I>
        friend consteval std::array<Block, N> internal::make_layout_impl(
            const std::array<Block, N>&,
            const std::array<uint32_t, N>&,
            std::index_sequence<I...>);

        template<size_t N>
        friend consteval std::array<Block, N> make_layout(const std::array<Block, N>&);

        // Private copy constructor that stamps in the computed offset from layout start.
        // Only callable by make_layout (via friend).
        consteval Block(const Block& other, uint32_t block_offset)
            : _sections(other._sections)
            , BLOCK_OFFSET(block_offset)
        {}

        std::span<const Section> _sections;
        const uint32_t           BLOCK_OFFSET;
    };

    namespace internal
    {
        template<size_t N, size_t... I>
        consteval std::array<Section, N> make_block_impl(
            const std::array<Section, N>&  sections,
            const std::array<uint32_t, N>& addresses,
            std::index_sequence<I...>)
        {
            return { Section(sections[I], addresses[I])... };
        }

        template<size_t N, size_t... I>
        consteval std::array<Block, N> make_layout_impl(
            const std::array<Block, N>&    blocks,
            const std::array<uint32_t, N>& offsets,
            std::index_sequence<I...>)
        {
            return { Block(blocks[I], offsets[I])... };
        }
    }    // namespace internal

    template<size_t N>
    consteval std::array<Section, N> make_block(const std::array<Section, N>& sections)
    {
        std::array<uint32_t, N> addresses{};

        for (size_t i = 1; i < N; i++)
        {
            addresses[i] = addresses[i - 1] +
                           section_byte_size(sections[i - 1].NUMBER_OF_PARAMETERS,
                                             sections[i - 1].PARAMETER_TYPE);
        }

        return internal::make_block_impl(sections, addresses, std::make_index_sequence<N>{});
    }

    // definition — see forward declaration above for documentation
    template<size_t N>
    consteval std::array<Block, N> make_layout(const std::array<Block, N>& blocks)
    {
        std::array<uint32_t, N> offsets{};

        for (size_t i = 1; i < N; i++)
        {
            offsets[i] = offsets[i - 1] + blocks[i - 1].byte_size();
        }

        return internal::make_layout_impl(blocks, offsets, std::make_index_sequence<N>{});
    }
}    // namespace zlibs::utils::lessdb
