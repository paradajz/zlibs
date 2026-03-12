/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "datetime.h"
#include "noncopyable.h"
#include "userdata_struct.h"

#include <functional>

namespace zlibs::utils::misc
{
    /**
     * @brief Delayable Zephyr kwork item bound to a C++ callback.
     *
     * The work item uses Zephyr's system workqueue unless a custom queue
     * resolver is provided. The resolver is evaluated when work is scheduled,
     * so queue selection is deferred until `reschedule()`.
     */
    class KworkDelayable : public NonCopyableOrMovable
    {
        public:
        using KworkDelayableExtended = UserDataStruct<k_work_delayable, KworkDelayable>;
        using WorkqueueFunction      = std::function<void()>;
        using WorkqueueResolver      = std::function<k_work_q*()>;

        /**
         * @brief Creates a delayable work item.
         *
         * Uses Zephyr's system workqueue.
         *
         * @param cb Callback executed when work runs.
         */
        explicit KworkDelayable(WorkqueueFunction&& cb)
            : _cb(std::move(cb))
        {
            init_delayable_work();
        }

        /**
         * @brief Creates a delayable work item that resolves its target queue on schedule.
         *
         * @param cb Callback executed when work runs.
         * @param resolver Optional queue resolver. Returning `nullptr` uses the system queue.
         */
        explicit KworkDelayable(WorkqueueFunction&& cb, WorkqueueResolver&& resolver)
            : _cb(std::move(cb))
            , _workqueue_resolver(std::move(resolver))
        {
            init_delayable_work();
        }

        /**
         * @brief Cancels pending work on destruction.
         */
        ~KworkDelayable()
        {
            cancel();
        }

        /**
         * @brief Reschedules the work item.
         *
         * `Ms` is the default unit.
         *
         * @param duration Delay duration.
         * @param unit Time unit for `duration`.
         *
         * @return `true` when schedule call succeeds and updates pending work, otherwise `false`.
         */
        bool reschedule(uint32_t duration, TimerUnit unit = TimerUnit::Ms)
        {
            switch (unit)
            {
            case TimerUnit::Us:
                return reschedule(duration ? K_USEC(duration) : K_NO_WAIT);

            case TimerUnit::Ms:
                return reschedule(duration ? K_MSEC(duration) : K_NO_WAIT);

            case TimerUnit::Sec:
                return reschedule(duration ? K_SECONDS(duration) : K_NO_WAIT);

            default:
                return false;
            }
        }

        /**
         * @brief Reschedules the work item.
         *
         * @param timeout Delay as Zephyr timeout.
         *
         * @return `true` when schedule call succeeds and updates pending work, otherwise `false`.
         */
        bool reschedule(k_timeout_t timeout)
        {
            auto* workqueue = resolved_workqueue();

            if (!workqueue)
            {
                return k_work_reschedule(&_e_kwork.member,
                                         timeout) > 0;
            }

            return k_work_reschedule_for_queue(workqueue,
                                               &_e_kwork.member,
                                               timeout) > 0;
        }

        /**
         * @brief Cancels pending delayed work and waits for completion.
         *
         * If the handler is already running, this call blocks until it becomes
         * idle. It must not be invoked from the work item handler itself.
         */
        void cancel()
        {
            k_work_sync sync = {};
            k_work_cancel_delayable_sync(&_e_kwork.member, &sync);
        }

        private:
        void init_delayable_work()
        {
            _e_kwork.user_data = this;
            k_work_init_delayable(&_e_kwork.member, [](k_work* work)
                                  {
                                      auto self = KworkDelayableExtended::extract_user_data(work);

                                      if (self == nullptr)
                                      {
                                          return;
                                      }

                                      if (self->_cb != nullptr)
                                      {
                                          self->_cb();
                                      }
                                  });
        }

        k_work_q* resolved_workqueue() const
        {
            return _workqueue_resolver ? _workqueue_resolver() : _workqueue;
        }

        KworkDelayableExtended _e_kwork            = {};
        WorkqueueFunction      _cb                 = nullptr;
        k_work_q*              _workqueue          = nullptr;
        WorkqueueResolver      _workqueue_resolver = nullptr;
    };
}    // namespace zlibs::utils::misc
