/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#include "zlibs/utils/signaling/signaling.h"

namespace zlibs::utils::signaling
{
    Signaling& Signaling::instance()
    {
        static Signaling signaling;
        return signaling;
    }

    Signaling::Signaling()
        : _thread([this]()
                  {
                      run_loop();
                  },
                  [this]()
                  {
                      request_stop();
                  })
    {
        k_fifo_init(&_fifo);
        k_mem_slab_init(&_slab, _slab_buffer, sizeof(signaling::DispatchNode), CONFIG_ZLIBS_UTILS_SIGNALING_MAX_POOL_SIZE);
        _lifecycle.store(Lifecycle::Running, std::memory_order_release);
        _thread.run();
    }

    void Signaling::run_loop()
    {
        _dispatcher_thread.store(k_current_get(), std::memory_order_release);

        while (true)
        {
            auto* node = static_cast<signaling::DispatchNode*>(k_fifo_get(&_fifo, K_FOREVER));

            if (node == nullptr)
            {
                continue;
            }

            node->invoke(node);
            release(node);

            if (_lifecycle.load(std::memory_order_acquire) != Lifecycle::Running)
            {
                break;
            }
        }

        while (auto* entry = static_cast<signaling::DispatchNode*>(k_fifo_get(&_fifo, K_NO_WAIT)))
        {
            entry->invoke(entry);
            release(entry);
        }

        _dispatcher_thread.store(nullptr, std::memory_order_release);
        _lifecycle.store(Lifecycle::Stopped, std::memory_order_release);
    }

    void Signaling::request_stop()
    {
        _lifecycle.store(Lifecycle::Stopping, std::memory_order_release);

        if (auto* memory = allocate(K_FOREVER))
        {
            auto* node   = new (memory) DispatchNode{};
            node->invoke = &Signaling::noop;
            k_fifo_put(&_fifo, node);
        }
    }

    void Signaling::noop([[maybe_unused]] DispatchNode* node)
    {}

    bool Signaling::try_enqueue(DispatchNode* node)
    {
        if (_lifecycle.load(std::memory_order_acquire) != Lifecycle::Running)
        {
            return false;
        }

        const zlibs::utils::misc::LockGuard lock(_enqueue_lock);

        if (_lifecycle.load(std::memory_order_acquire) != Lifecycle::Running)
        {
            return false;
        }

        _enqueue_generation++;
        k_fifo_put(&_fifo, node);
        return true;
    }

    bool Signaling::drain(k_timeout_t alloc_timeout)
    {
        if (_lifecycle.load(std::memory_order_acquire) != Lifecycle::Running)
        {
            return false;
        }

        while (true)
        {
            uint64_t before = 0;

            if (!enqueue_drain_barrier(alloc_timeout, before))
            {
                return false;
            }

            uint64_t after = 0;

            {
                const zlibs::utils::misc::LockGuard lock(_enqueue_lock);
                after = _enqueue_generation;
            }

            if (before == after)
            {
                return true;
            }
        }
    }

    bool Signaling::enqueue_drain_barrier(k_timeout_t alloc_timeout, uint64_t& generation)
    {
        static_assert(sizeof(DrainPayload) <= CONFIG_ZLIBS_UTILS_SIGNALING_MAX_PAYLOAD_LENGTH_BYTES,
                      "DrainPayload too large for dispatcher payload; increase CONFIG_ZLIBS_UTILS_SIGNALING_MAX_PAYLOAD_LENGTH_BYTES.");

        k_sem done = {};
        k_sem_init(&done, 0, 1);

        auto* memory = allocate(alloc_timeout);

        if (memory == nullptr)
        {
            return false;
        }

        auto* node   = new (memory) DispatchNode{};
        node->invoke = &DrainJob::invoke;
        new (node->payload) DrainPayload{ .done = &done };

        {
            const zlibs::utils::misc::LockGuard lock(_enqueue_lock);

            if (_lifecycle.load(std::memory_order_acquire) != Lifecycle::Running)
            {
                std::launder(reinterpret_cast<DrainPayload*>(node->payload))->~DrainPayload();
                release(node);
                return false;
            }

            generation = _enqueue_generation;
            k_fifo_put(&_fifo, node);
        }

        k_sem_take(&done, K_FOREVER);

        return true;
    }

    bool Signaling::dispatch_sync_impl(DispatchSyncCallback cb, void* context, k_timeout_t alloc_timeout)
    {
        static_assert(sizeof(DispatchSyncPayload) <= CONFIG_ZLIBS_UTILS_SIGNALING_MAX_PAYLOAD_LENGTH_BYTES,
                      "DispatchSyncPayload too large for dispatcher payload; increase CONFIG_ZLIBS_UTILS_SIGNALING_MAX_PAYLOAD_LENGTH_BYTES.");

        if (_lifecycle.load(std::memory_order_acquire) != Lifecycle::Running)
        {
            return false;
        }

        k_sem done = {};
        k_sem_init(&done, 0, 1);

        auto* memory = allocate(alloc_timeout);

        if (memory == nullptr)
        {
            return false;
        }

        auto* node   = new (memory) DispatchNode{};
        node->invoke = &DispatchSyncJob::invoke;
        new (node->payload) DispatchSyncPayload{
            .done    = &done,
            .cb      = cb,
            .context = context,
        };

        if (!try_enqueue(node))
        {
            std::launder(reinterpret_cast<DispatchSyncPayload*>(node->payload))->~DispatchSyncPayload();
            release(node);
            return false;
        }

        k_sem_take(&done, K_FOREVER);

        return true;
    }

    void Signaling::release(DispatchNode* node)
    {
        if (node == nullptr)
        {
            return;
        }

        node->~DispatchNode();
        k_mem_slab_free(&_slab, node);
    }

    void* Signaling::allocate(k_timeout_t timeout)
    {
        void* memory = nullptr;
        return (k_mem_slab_alloc(&_slab, &memory, timeout) == 0) ? memory : nullptr;
    }
}    // namespace zlibs::utils::signaling
