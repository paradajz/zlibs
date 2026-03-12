/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace zlibs::utils::misc
{
    /**
     * @brief Fixed-size circular buffer with single-producer/single-consumer
     *        lock-free index updates.
     *
     * @tparam MaxSize Usable buffer capacity.
     * @tparam OverwriteOldest When set to `true`, inserts on a full buffer
     * discard the oldest entry so the newest value is always retained.
     * @tparam T Stored element type. Must be trivially copyable.
     */
    template<size_t MaxSize, bool OverwriteOldest = false, typename T = uint8_t>
    class RingBuffer
    {
        static_assert(MaxSize > 0, "Ring buffer size must be larger than 0.");
        static_assert(std::is_trivially_copyable_v<T>, "RingBuffer element type must be trivially copyable.");

        static constexpr size_t STORAGE_SIZE = MaxSize + 1;

        public:
        RingBuffer() = default;

        size_t size() const
        {
            const size_t head = _head.load(std::memory_order_acquire);
            const size_t tail = _tail.load(std::memory_order_acquire);

            if (head >= tail)
            {
                return head - tail;
            }

            return STORAGE_SIZE + head - tail;
        }

        bool is_empty() const
        {
            return _head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire);
        }

        bool is_full() const
        {
            return size() == MaxSize;
        }

        bool insert(T data)
        {
            const size_t head = _head.load(std::memory_order_relaxed);
            size_t       next = head + 1;

            if (next >= STORAGE_SIZE)
            {
                next = 0;
            }

            // Avoid overflow - this would create empty buffer.
            if (_tail.load(std::memory_order_acquire) == next)
            {
                if constexpr (!OverwriteOldest)
                {
                    return false;
                }

                size_t new_tail = _tail.load(std::memory_order_relaxed) + 1;

                if (new_tail >= STORAGE_SIZE)
                {
                    new_tail = 0;
                }

                // Retire the oldest item before publishing the new head.
                _tail.store(new_tail, std::memory_order_release);
            }

            _buffer[next] = data;
            _head.store(next, std::memory_order_release);

            return true;
        }

        std::optional<T> peek() const
        {
            const size_t head = _head.load(std::memory_order_acquire);
            const size_t tail = _tail.load(std::memory_order_relaxed);

            if (head == tail)
            {
                return {};
            }

            size_t next = tail + 1;

            if (next >= STORAGE_SIZE)
            {
                next = 0;
            }

            return _buffer[next];
        }

        std::optional<T> remove()
        {
            const size_t head = _head.load(std::memory_order_acquire);
            const size_t tail = _tail.load(std::memory_order_relaxed);

            if (head == tail)
            {
                return {};
            }

            size_t next = tail + 1;

            if (next >= STORAGE_SIZE)
            {
                next = 0;
            }

            const T value = _buffer[next];
            _tail.store(next, std::memory_order_release);

            return value;
        }

        void reset()
        {
            _tail.store(_head.load(std::memory_order_acquire), std::memory_order_release);
        }

        protected:
        T                   _buffer[STORAGE_SIZE] = {};
        std::atomic<size_t> _head                 = 0;
        std::atomic<size_t> _tail                 = 0;
    };
}    // namespace zlibs::utils::misc
