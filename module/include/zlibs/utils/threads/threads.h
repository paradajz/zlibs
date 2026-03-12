/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/misc/noncopyable.h"
#include "zlibs/utils/misc/string.h"
#include "zlibs/utils/misc/userdata_struct.h"

#include <atomic>
#include <functional>

namespace zlibs::utils::threads
{
    /**
     * @brief Storage block used by a Zephyr thread instance.
     *
     * @tparam StackSize Thread stack size in bytes.
     */
    template<int StackSize>
    struct ThreadStorage    // NOLINT
    {
        static_assert(StackSize > 0, "stackSize must be greater than 0");

        K_KERNEL_STACK_MEMBER(stack, StackSize);
        k_thread thread = {};
        k_tid_t  tid    = {};
    };

    /**
     * @brief Base type holding thread storage and compile-time name metadata.
     *
     * @tparam Name Thread name as compile-time string literal.
     * @tparam StackSize Thread stack size in bytes.
     */
    template<misc::StringLiteral Name, size_t StackSize>
    class BaseThread
    {
        public:
        BaseThread() = default;

        /**
         * @brief Returns the compile-time thread name.
         *
         * @return Thread name as `std::string_view`.
         */
        static constexpr std::string_view name()
        {
            return Name.value.data();
        }

        protected:
        ThreadStorage<StackSize> _storage = {};
    };

    /**
     * @brief Wrapper for a user-provided loop callback running in a Zephyr thread.
     *
     * @tparam Name Thread name as compile-time string literal.
     * @tparam Priority Zephyr thread priority.
     * @tparam StackSize Thread stack size in bytes.
     */
    template<misc::StringLiteral Name, int Priority, size_t StackSize>
    class UserThread : public BaseThread<Name, StackSize>, zlibs::utils::misc::NonCopyableOrMovable
    {
        public:
        /** @brief Base-thread specialization used by this user thread wrapper. */
        using BaseThreadType = BaseThread<Name, StackSize>;

        /** @brief Callback signature used for thread loop and exit handlers. */
        using ThreadCallback = std::function<void()>;

        /**
         * @brief Creates a thread with a loop callback.
         *
         * @param loop_cb Callback executed when the thread runs.
         */
        explicit UserThread(ThreadCallback&& loop_cb)
        {
            if (create())
            {
                _loop_cb = std::move(loop_cb);
            }
        }

        /**
         * @brief Creates a thread with loop and exit callbacks.
         *
         * @param loop_cb Callback executed when the thread runs.
         * @param exit_cb Callback executed before thread join/abort during destruction.
         */
        UserThread(ThreadCallback&& loop_cb, ThreadCallback&& exit_cb)
        {
            if (create())
            {
                _loop_cb = std::move(loop_cb);
                _exit_cb = std::move(exit_cb);
            }
        }

        /**
         * @brief Stops and cleans up the underlying thread.
         */
        ~UserThread()
        {
            destroy();
        }

        /**
         * @brief Starts/resumes the thread execution.
         *
         * @return `true` if thread is valid and resumed, otherwise `false`.
         */
        bool run()
        {
            if (!_created.load(std::memory_order_acquire) || !_loop_cb)
            {
                return false;
            }

            k_thread_resume(this->_storage.tid);
            k_wakeup(this->_storage.tid);

            return true;
        }

        private:
        std::atomic<bool> _created = { false };
        ThreadCallback    _loop_cb = nullptr;
        ThreadCallback    _exit_cb = nullptr;

        /**
         * @brief Creates and configures underlying Zephyr thread.
         *
         * @return `true` when thread is created, otherwise `false`.
         */
        bool create()
        {
            this->_storage.tid = k_thread_create(
                &this->_storage.thread,
                this->_storage.stack,
                K_THREAD_STACK_SIZEOF(this->_storage.stack),
                [](void* p1, void*, void*)
                {
                    auto self = static_cast<UserThread*>(p1);

                    if (self == nullptr)
                    {
                        return;
                    }

                    if (self->_loop_cb)
                    {
                        self->_loop_cb();
                    }
                },
                this,
                nullptr,
                nullptr,
                Priority,
                0,
                K_FOREVER);

            if (this->_storage.tid == nullptr)
            {
                return false;
            }

#ifdef CONFIG_THREAD_NAME
            k_thread_name_set(this->_storage.tid, BaseThreadType::name().data());
#endif

            _created.store(true, std::memory_order_release);
            return true;
        }

        /**
         * @brief Stops thread and joins it, aborting on join timeout.
         *
         * @param timeout Maximum join wait time.
         */
        void destroy(k_timeout_t timeout = K_MSEC(100))
        {
            if (!_created.exchange(false))
            {
                return;
            }

            k_thread_resume(this->_storage.tid);

            if (_exit_cb)
            {
                _exit_cb();
            }

            int ret = k_thread_join(this->_storage.tid, timeout);

            if (ret != 0)
            {
                k_thread_abort(this->_storage.tid);
                k_thread_join(this->_storage.tid, K_FOREVER);
            }
        }
    };

    /**
     * @brief Singleton Zephyr workqueue thread wrapper.
     *
     * @tparam Name Workqueue thread name.
     * @tparam Priority Workqueue thread priority.
     * @tparam StackSize Workqueue stack size in bytes.
     */
    template<misc::StringLiteral Name, int Priority, size_t StackSize>
    class WorkqueueThread : public BaseThread<Name, StackSize>, zlibs::utils::misc::NonCopyableOrMovable
    {
        public:
        /** @brief Base-thread specialization used by this workqueue wrapper. */
        using BaseThreadType = BaseThread<Name, StackSize>;

        /**
         * @brief Stops the workqueue thread on destruction.
         */
        ~WorkqueueThread()
        {
            destroy();
        }

        /**
         * @brief Returns the singleton workqueue handle.
         *
         * @return Pointer to internal `k_work_q`.
         */
        static k_work_q* handle()
        {
            return &instance()._workqueue;
        }

        private:
        std::atomic<bool> _created   = { false };
        k_work_q          _workqueue = {};

        /**
         * @brief Constructs singleton workqueue thread and starts queue.
         */
        WorkqueueThread()
        {
            create();
        }

        /**
         * @brief Returns singleton workqueue thread instance.
         *
         * @return Singleton instance reference.
         */
        static WorkqueueThread& instance()
        {
            static WorkqueueThread instance;
            return instance;
        }

        /**
         * @brief Creates and starts Zephyr workqueue.
         *
         * @return `true` when queue exists or creation succeeds.
         */
        bool create()
        {
            if (_created.load(std::memory_order_acquire))
            {
                return true;
            }

            k_work_queue_init(&_workqueue);
            k_work_queue_start(&_workqueue,
                               this->_storage.stack,
                               K_THREAD_STACK_SIZEOF(this->_storage.stack),
                               Priority,
                               nullptr);

            _created.store(true, std::memory_order_release);

            return true;
        }

        /**
         * @brief Drains and stops Zephyr workqueue.
         */
        void destroy()
        {
            if (!_created.load(std::memory_order_acquire))
            {
                return;
            }

            k_work_queue_drain(&_workqueue, true);
            k_work_queue_stop(&_workqueue, K_FOREVER);
            _created.store(false, std::memory_order_release);
        }
    };
}    // namespace zlibs::utils::threads
