/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <optional>

namespace zlibs::utils::misc
{
    /**
     * @brief Abstract interface for ring buffer.
     */
    class RingBufferBase
    {
        public:
        RingBufferBase()          = default;
        virtual ~RingBufferBase() = default;

        /**
         * @brief Gets the number of elements currently stored.
         *
         * @return Number of stored bytes.
         */
        virtual size_t size() = 0;

        /**
         * @brief Checks whether the buffer is empty.
         *
         * @return `true` if empty, otherwise `false`.
         */
        virtual bool is_empty() = 0;

        /**
         * @brief Checks whether the buffer is full.
         *
         * @return `true` if full, otherwise `false`.
         */
        virtual bool is_full() = 0;

        /**
         * @brief Inserts one byte into the buffer.
         *
         * @param data Byte to insert.
         *
         * @return `true` on success, `false` if buffer is full.
         */
        virtual bool insert(uint8_t data) = 0;

        /**
         * @brief Reads the next element without removing it.
         *
         * @return The next element when available, otherwise std::nullopt.
         */
        virtual std::optional<uint8_t> peek() = 0;

        /**
         * @brief Removes and returns the next byte.
         *
         * @return The removed byte when available, otherwise `std::nullopt`.
         */
        virtual std::optional<uint8_t> remove() = 0;

        /**
         * @brief Clears all buffered data.
         */
        virtual void reset() = 0;
    };

    /**
     * @brief Fixed-size circular byte buffer with single-producer/single-consumer
     *        lock-free index updates.
     *
     * @tparam MaxSize Backing storage size. One slot is reserved to distinguish
     * empty from full, so the usable capacity is `MaxSize - 1`. Must be a power
     * of two and greater than 1.
     */
    template<size_t MaxSize>
    class RingBuffer : public RingBufferBase
    {
        static_assert(MaxSize > 1 && !(MaxSize & (MaxSize - 1)), "Ring buffer size must be a power of two and larger than 1.");

        public:
        RingBuffer() = default;

        size_t size() override
        {
            const size_t HEAD = _head.load(std::memory_order_acquire);
            const size_t TAIL = _tail.load(std::memory_order_acquire);

            if (HEAD >= TAIL)
            {
                return HEAD - TAIL;
            }

            return MaxSize + HEAD - TAIL;
        }

        bool is_empty() override
        {
            return _head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire);
        }

        bool is_full() override
        {
            return size() == (MaxSize - 1);
        }

        bool insert(uint8_t data) override
        {
            const size_t HEAD = _head.load(std::memory_order_relaxed);
            const size_t NEXT = (HEAD + 1) & (MaxSize - 1);

            // avoid overflow - this would create empty buffer
            if (_tail.load(std::memory_order_acquire) == NEXT)
            {
                return false;
            }

            _buffer[NEXT] = data;
            _head.store(NEXT, std::memory_order_release);

            return true;
        }

        std::optional<uint8_t> peek() override
        {
            const size_t HEAD = _head.load(std::memory_order_acquire);
            const size_t TAIL = _tail.load(std::memory_order_relaxed);

            if (HEAD == TAIL)
            {
                return {};
            }

            size_t next = TAIL + 1;

            if (next >= MaxSize)
            {
                next = 0;
            }

            return _buffer[next];
        }

        std::optional<uint8_t> remove() override
        {
            const size_t HEAD = _head.load(std::memory_order_acquire);
            const size_t TAIL = _tail.load(std::memory_order_relaxed);

            if (HEAD == TAIL)
            {
                return {};
            }

            size_t next = TAIL + 1;

            if (next >= MaxSize)
            {
                next = 0;
            }

            const uint8_t VALUE = _buffer[next];
            _tail.store(next, std::memory_order_release);

            return VALUE;
        }

        void reset() override
        {
            _tail.store(_head.load(std::memory_order_acquire), std::memory_order_release);
        }

        private:
        uint8_t             _buffer[MaxSize] = {};
        std::atomic<size_t> _head            = 0;
        std::atomic<size_t> _tail            = 0;
    };
}    // namespace zlibs::utils::misc
