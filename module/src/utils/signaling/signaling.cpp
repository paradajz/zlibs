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

        k_fifo_put(&_fifo, node);
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
