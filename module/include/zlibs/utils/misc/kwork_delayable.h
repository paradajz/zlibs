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
     */
    class KworkDelayable : public NonCopyableOrMovable
    {
        public:
        using KworkDelayableExtended = UserDataStruct<k_work_delayable, KworkDelayable>;
        using WorkqueueFunction      = std::function<void()>;

        /**
         * @brief Creates a delayable work item.
         *
         * @param cb Callback executed when work runs.
         * @param workqueue Optional target queue. Uses system queue when `nullptr`.
         */
        explicit KworkDelayable(WorkqueueFunction&& cb, k_work_q* workqueue = nullptr)
            : _cb(std::move(cb))
            , _workqueue(workqueue)
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
            if (!_workqueue)
            {
                return k_work_reschedule(&_e_kwork.member,
                                         timeout) > 0;
            }

            return k_work_reschedule_for_queue(_workqueue,
                                               &_e_kwork.member,
                                               timeout) > 0;
        }

        /**
         * @brief Cancels pending delayed work.
         */
        void cancel()
        {
            k_work_cancel_delayable(&_e_kwork.member);
        }

        private:
        KworkDelayableExtended _e_kwork   = {};
        WorkqueueFunction      _cb        = nullptr;
        k_work_q*              _workqueue = nullptr;
    };
}    // namespace zlibs::utils::misc
