/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "zlibs/utils/misc/mutex.h"
#include "zlibs/utils/misc/noncopyable.h"
#include "zlibs/utils/threads/threads.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace zlibs::utils::signaling
{
    /**
     * @brief RAII subscription handle that auto-unsubscribes on destruction.
     */
    class Subscription
    {
        public:
        /** @brief Callback signature used to unsubscribe by id. */
        using UnsubCb = void (*)(uint64_t);

        Subscription() = default;

        /**
         * @brief Constructs a valid subscription handle.
         *
         * @param unsub_cb Callback used to unsubscribe.
         * @param id Subscription identifier.
         */
        Subscription(UnsubCb unsub_cb, uint64_t id)
            : _unsub_cb(unsub_cb)
            , _id(id)
        {}

        Subscription(Subscription const&)            = delete;
        Subscription& operator=(Subscription const&) = delete;

        /**
         * @brief Move-constructs a subscription, transferring ownership.
         *
         * @param other Source subscription.
         */
        Subscription(Subscription&& other) noexcept
        {
            *this = std::move(other);
        }

        /**
         * @brief Move-assigns a subscription, transferring ownership.
         *
         * @param other Source subscription.
         *
         * @return Reference to this object.
         */
        Subscription& operator=(Subscription&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                _unsub_cb       = other._unsub_cb;
                _id             = other._id;
                other._unsub_cb = nullptr;
                other._id       = 0;
            }

            return *this;
        }

        /**
         * @brief Unsubscribes if still active.
         */
        ~Subscription()
        {
            reset();
        }

        /**
         * @brief Unsubscribes and clears this handle.
         */
        void reset()
        {
            if (_unsub_cb)
            {
                _unsub_cb(_id);
            }

            _unsub_cb = nullptr;
            _id       = 0;
        }

        private:
        UnsubCb  _unsub_cb = nullptr;
        uint64_t _id       = 0;
    };

    /**
     * @brief Functional signal stream view with composable operators.
     *
     * @tparam T Signal payload type.
     */
    template<typename T>
    class Observable
    {
        public:
        /** @brief Observer callback receiving each emitted value. */
        using Callback = std::function<void(T const&)>;

        /**
         * @brief Creates an observable from a subscribe callback.
         *
         * @param sub_cb Callback that attaches an observer and returns a subscription.
         */
        explicit Observable(std::function<Subscription(Callback)> sub_cb)
            : _sub_cb(std::move(sub_cb))
        {}

        /**
         * @brief Subscribes to this observable.
         *
         * @param cb Callback to invoke for each value.
         *
         * @return Subscription handle.
         */
        [[nodiscard]] Subscription subscribe(Callback cb) const
        {
            return _sub_cb(std::move(cb));
        }

        /**
         * @brief Maps values to another type.
         *
         * @tparam F Callable type.
         * @param f Transformation callable.
         *
         * @return New observable of transformed values.
         */
        template<typename F>
        auto transform(F f) const
        {
            using U  = std::decay_t<decltype(f(std::declval<T const&>()))>;
            auto src = *this;

            return Observable<U>(
                [src, f](std::function<void(U const&)> out_cb) mutable -> Subscription
                {
                    return src.subscribe(
                        [f, out_cb = std::move(out_cb)](T const& v) mutable
                        {
                            out_cb(f(v));
                        });
                });
        }

        /**
         * @brief Emits only values passing a predicate.
         *
         * @tparam P Predicate callable type.
         * @param pred Predicate returning `true` for values to keep.
         *
         * @return Filtered observable.
         */
        template<typename P>
        auto only_if(P pred) const
        {
            auto src = *this;

            return Observable<T>(
                [src, pred](Callback out_cb) mutable -> Subscription
                {
                    return src.subscribe(
                        [pred, out_cb = std::move(out_cb)](T const& v) mutable
                        {
                            if (pred(v))
                            {
                                out_cb(v);
                            }
                        });
                });
        }

        /**
         * @brief Reduces stream values into an accumulated state.
         *
         * @tparam Acc Accumulator state type.
         * @tparam Reducer Reducer callable type.
         * @param init Initial accumulator value.
         * @param r Reducer callable `(state, value) -> new_state`.
         *
         * @return Observable emitting updated accumulator state.
         */
        template<typename Acc, typename Reducer>
        auto accumulate(Acc init, Reducer r) const
        {
            using Out = std::decay_t<Acc>;
            auto src  = *this;

            return Observable<Out>(
                [src, init = Out(std::move(init)), r](auto out_cb) mutable -> Subscription
                {
                    auto state = std::make_shared<Out>(std::move(init));    // per subscription
                    return src.subscribe([state, r, out_cb = std::move(out_cb)](T const& v) mutable
                                         {
                                             *state = r(*state, v);
                                             out_cb(*state);
                                         });
                });
        }

        /**
         * @brief Emits only when value changes compared to previous emission.
         *
         * @return Observable with deduplicated consecutive values.
         */
        auto only_on_change() const
        {
            auto src = *this;

            return Observable<T>(
                [src](Callback out_cb) mutable -> Subscription
                {
                    auto last = std::make_shared<std::optional<T>>();
                    return src.subscribe(
                        [last, out_cb = std::move(out_cb)](T const& v) mutable
                        {
                            if (!last->has_value() || !(v == **last))
                            {
                                *last = v;
                                out_cb(v);
                            }
                        });
                });
        }

        private:
        std::function<Subscription(Callback)> _sub_cb = nullptr;
    };

    /**
     * @brief Static in-process pub/sub channel for a signal type.
     *
     * @tparam Signal Signal payload type.
     */
    template<typename Signal>
    class Channel
    {
        public:
        Channel() = default;

        static_assert(std::is_copy_constructible_v<Signal>,
                      "Signal must be copy-constructible.");

        /** @brief Subscriber callback signature for a channel payload type. */
        using Callback = std::function<void(Signal const&)>;

        /**
         * @brief Subscribes a callback to this signal channel.
         *
         * @param cb Callback invoked on publish.
         * @param on_sub_cb Optional internal hook used to schedule replay after
         *                  registration when cached signal data exists.
         *
         * @return Subscription handle.
         */
        [[nodiscard]] static Subscription subscribe(Callback cb, const std::function<void()>& on_sub_cb = nullptr)
        {
            __ASSERT(publish_depth == 0, "subscribe called from within publish callback");

            const zlibs::utils::misc::LockGuard LOCK(mutex);

            cleanup_subs();

            const uint64_t ID            = ++next_id;
            const bool     SHOULD_REPLAY = last_signal.has_value();

            subscribers.push_back(Slot{ ID, std::move(cb), SHOULD_REPLAY });

            if (on_sub_cb && SHOULD_REPLAY)
            {
                on_sub_cb();
            }

            return Subscription{ &unsubscribe, ID };
        }

        /**
         * @brief Unsubscribes a callback by id.
         *
         * @param id Subscription identifier.
         */
        static void unsubscribe(uint64_t id)
        {
            __ASSERT(publish_depth == 0, "unsubscribe called from within publish callback");

            const zlibs::utils::misc::LockGuard LOCK(mutex);

            for (auto& s : subscribers)
            {
                if (s.id == id && s.cb)
                {
                    s.cb             = nullptr;
                    cleanup_required = true;
                    break;
                }
            }

            cleanup_subs();
        }

        /**
         * @brief Publishes a signal to all current subscribers.
         *
         * @param signal Signal payload.
         */
        static void publish(const Signal& signal)
        {
            const zlibs::utils::misc::LockGuard LOCK(mutex);

            last_signal = signal;

            publish_depth++;

            for (auto& s : subscribers)
            {
                if (s.cb)
                {
                    s.needs_replay = false;
                    s.cb(signal);
                }
            }

            publish_depth--;

            cleanup_subs();
        }

        /**
         * @brief Removes invalidated subscribers once publishing is not in progress.
         */
        static void cleanup_subs()
        {
            if (!cleanup_required || publish_depth != 0)
            {
                return;
            }

            auto it = std::remove_if(subscribers.begin(), subscribers.end(), [](Slot const& s)
                                     {
                                         return s.cb == nullptr;
                                     });

            subscribers.erase(it, subscribers.end());
            cleanup_required = false;
        }

        /**
         * @brief Replays the last signal once to subscribers that require replay.
         */
        static void replay()
        {
            const zlibs::utils::misc::LockGuard LOCK(mutex);

            if (last_signal.has_value())
            {
                publish_depth++;

                for (auto& s : subscribers)
                {
                    if (s.cb && s.needs_replay)
                    {
                        s.needs_replay = false;
                        s.cb(*last_signal);
                    }
                }

                publish_depth--;
            }

            cleanup_subs();
        }

        /**
         * @brief Returns a copy of the most recently published signal.
         *
         * @return Last signal if available; otherwise `std::nullopt`.
         */
        static std::optional<Signal> last_signal_data()
        {
            const zlibs::utils::misc::LockGuard LOCK(mutex);
            auto                                ret = last_signal;

            return ret;
        }

        private:
        /**
         * @brief Internal subscriber slot metadata.
         */
        struct Slot
        {
            uint64_t id           = 0;
            Callback cb           = nullptr;
            bool     needs_replay = true;
        };

        static inline std::vector<Slot>         subscribers      = {};
        static inline uint64_t                  next_id          = 0;
        static inline std::optional<Signal>     last_signal      = {};
        static inline uint32_t                  publish_depth    = 0;
        static inline bool                      cleanup_required = false;
        static inline zlibs::utils::misc::Mutex mutex;
    };

    /**
     * @brief FIFO node used by the asynchronous signaling dispatcher.
     */
    struct DispatchNode
    {
        void* fifo_reserved           = nullptr;
        void (*invoke)(DispatchNode*) = nullptr;
        alignas(std::max_align_t) uint8_t payload[CONFIG_ZLIBS_UTILS_SIGNALING_MAX_PAYLOAD_LENGTH_BYTES];
    };

    /**
     * @brief Job adapter that publishes a queued signal payload.
     *
     * @tparam Signal Signal payload type.
     */
    template<typename Signal>
    struct PublishJob
    {
        /**
         * @brief Publishes payload stored in a dispatch node.
         *
         * @param node Dispatch node containing `Signal` payload.
         */
        static void invoke(DispatchNode* node)
        {
            auto t = std::launder(reinterpret_cast<Signal*>(node->payload));
            Channel<Signal>::publish(*t);
            t->~Signal();
        }
    };

    /**
     * @brief Job adapter that replays the latest signal for a channel.
     *
     * @tparam Signal Signal payload type.
     */
    template<typename Signal>
    struct ReplayJob
    {
        /**
         * @brief Replays last payload for channel subscribers.
         *
         * @param node Dispatch node (payload unused).
         */
        static void invoke([[maybe_unused]] DispatchNode* node)
        {
            Channel<Signal>::replay();
        }
    };

    /**
     * @brief Singleton asynchronous signaling dispatcher backed by a dedicated thread, FIFO and memory slab.
     */
    class Signaling : public zlibs::utils::misc::NonCopyableOrMovable
    {
        public:
        /**
         * @brief Returns the global signaling dispatcher instance.
         *
         * @return Singleton dispatcher reference.
         */
        static Signaling& instance()
        {
            static Signaling signaling;
            return signaling;
        }

        /**
         * @brief Queues a signal for asynchronous publishing.
         *
         * @tparam Signal Signal payload type.
         * @param signal Signal payload copy.
         *
         * @return `true` if queued; `false` if the dispatcher is stopping/stopped
         *         or if the pool is exhausted.
         */
        template<typename Signal>
        bool publish(const Signal& signal)
        {
            static_assert(sizeof(Signal) <= CONFIG_ZLIBS_UTILS_SIGNALING_MAX_PAYLOAD_LENGTH_BYTES,
                          "Signal too large for dispatcher payload; increase CONFIG_ZLIBS_UTILS_SIGNALING_MAX_PAYLOAD_LENGTH_BYTES.");
            static_assert(std::is_copy_constructible_v<Signal>,
                          "Signal must be copy-constructible.");
            static_assert(alignof(Signal) <= alignof(std::max_align_t),
                          "Signal alignment too large for dispatcher payload.");

            if (_lifecycle.load(std::memory_order_acquire) != Lifecycle::Running)
            {
                return false;
            }

            if (auto memory = allocate())
            {
                auto node    = new (memory) DispatchNode{};
                node->invoke = &PublishJob<Signal>::invoke;
                new (node->payload) Signal(signal);

                if (_lifecycle.load(std::memory_order_acquire) != Lifecycle::Running)
                {
                    std::launder(reinterpret_cast<Signal*>(node->payload))->~Signal();
                    node->~DispatchNode();
                    k_mem_slab_free(&_slab, node);
                    return false;
                }

                k_fifo_put(&_fifo, node);

                return true;
            }

            return false;
        }

        /**
         * @brief Subscribes to a signal and schedules replay of latest published signal.
         *
         * @tparam Signal Signal payload type.
         * @tparam Cb Callback callable type.
         * @param cb Subscriber callback.
         * @param replay_dropped Optional output flag set to `true` when cached
         *                       replay exists but cannot be queued.
         *
         * @return Subscription handle.
         */
        template<typename Signal, typename Cb>
        [[nodiscard]] Subscription subscribe(Cb&& cb, bool* replay_dropped = nullptr)
        {
            if (replay_dropped != nullptr)
            {
                *replay_dropped = false;
            }

            return Channel<Signal>::subscribe(
                typename Channel<Signal>::Callback{ std::forward<Cb>(cb) },
                [this, replay_dropped]()
                {
                    if (_lifecycle.load(std::memory_order_acquire) == Lifecycle::Running)
                    {
                        if (auto memory = allocate())
                        {
                            auto node    = new (memory) DispatchNode{};
                            node->invoke = &ReplayJob<Signal>::invoke;

                            if (_lifecycle.load(std::memory_order_acquire) == Lifecycle::Running)
                            {
                                k_fifo_put(&_fifo, node);
                                return;
                            }

                            node->~DispatchNode();
                            k_mem_slab_free(&_slab, node);
                        }
                    }

                    if (replay_dropped != nullptr)
                    {
                        *replay_dropped = true;
                    }
                });
        }

        /**
         * @brief Creates an observable view for a signal type.
         *
         * @tparam Signal Signal payload type.
         * @return Observable emitting this signal type.
         */
        template<typename Signal>
        Observable<Signal> observe()
        {
            return Observable<Signal>(
                [this](std::function<void(Signal const&)> cb)
                {
                    return subscribe<Signal>(std::move(cb));
                });
        }

        /**
         * @brief Reads the last published signal.
         *
         * @tparam Signal Signal payload type.
         * @return Last signal if one exists; otherwise `std::nullopt`.
         */
        template<typename Signal>
        [[nodiscard]] static std::optional<Signal> last_signal_data()
        {
            return Channel<Signal>::last_signal_data();
        }

        private:
        /**
         * @brief Starts signaling dispatcher thread and internal resources.
         */
        Signaling()
            : _thread([this]()
                      {
                          while (true)
                          {
                              auto node = static_cast<signaling::DispatchNode*>(k_fifo_get(&_fifo, K_FOREVER));
                              node->invoke(node);
                              node->~DispatchNode();

                              k_mem_slab_free(&_slab, node);

                              if (_lifecycle.load(std::memory_order_acquire) != Lifecycle::Running)
                              {
                                  break;
                              }
                          }

                          // drain any leftover items without blocking
                          while (auto entry = k_fifo_get(&_fifo, K_NO_WAIT))
                          {
                              auto node = static_cast<signaling::DispatchNode*>(entry);
                              node->invoke(node);
                              node->~DispatchNode();
                              k_mem_slab_free(&_slab, node);
                          }

                          _lifecycle.store(Lifecycle::Stopped, std::memory_order_release);
                      },
                      [this]()
                      {
                          _lifecycle.store(Lifecycle::Stopping, std::memory_order_release);

                          if (auto memory = allocate(K_FOREVER))
                          {
                              auto node    = new (memory) DispatchNode{};
                              node->invoke = [](DispatchNode*)
                              {
                              };

                              k_fifo_put(&_fifo, node);
                          }
                      })
        {
            k_fifo_init(&_fifo);
            k_mem_slab_init(&_slab, _slab_buffer, sizeof(signaling::DispatchNode), CONFIG_ZLIBS_UTILS_SIGNALING_MAX_POOL_SIZE);
            _lifecycle.store(Lifecycle::Running, std::memory_order_release);
            _thread.run();
        }

        /** @brief Dedicated dispatcher thread type used by `Signaling`. */
        using Thread = zlibs::utils::threads::UserThread<zlibs::utils::misc::StringLiteral{ "zlibs_utils_signaling" },
                                                         K_PRIO_PREEMPT(CONFIG_ZLIBS_UTILS_SIGNALING_THREAD_PRIORITY),
                                                         CONFIG_ZLIBS_UTILS_SIGNALING_THREAD_STACK_SIZE>;

        enum class Lifecycle : uint8_t
        {
            Running,
            Stopping,
            Stopped,
        };

        Thread                 _thread;
        std::atomic<Lifecycle> _lifecycle = Lifecycle::Stopped;
        k_fifo                 _fifo      = {};
        k_mem_slab             _slab      = {};

        static_assert(sizeof(DispatchNode) % 4 == 0, "DispatchNode size must be multiple of 4 for k_mem_slab");
        char __aligned(alignof(std::max_align_t)) _slab_buffer[CONFIG_ZLIBS_UTILS_SIGNALING_MAX_POOL_SIZE * sizeof(signaling::DispatchNode)] = {};

        /**
         * @brief Allocates one dispatch node from the slab pool.
         *
         * @return Pointer to allocated memory, or `nullptr` on failure.
         */
        void* allocate(k_timeout_t timeout = K_NO_WAIT)
        {
            void* memory = nullptr;
            return (k_mem_slab_alloc(&_slab, &memory, timeout) == 0) ? memory : nullptr;
        }
    };

    /**
     * @brief Queues a signal through the global signaling dispatcher.
     *
     * @tparam Signal Signal payload type.
     * @param signal Signal payload copy.
     *
     * @return `true` if queued; `false` if the dispatcher is stopping/stopped
     *         or if the pool is exhausted.
     */
    template<typename Signal>
    bool publish(const Signal& signal)
    {
        return Signaling::instance().publish(signal);
    }

    /**
     * @brief Subscribes a callback through the global signaling dispatcher.
     *
     * @tparam Signal Signal payload type.
     * @tparam Cb Callback callable type.
     * @param cb Subscriber callback.
     * @param replay_dropped Optional output flag set to `true` when cached
     *                       replay exists but cannot be queued.
     * @return Subscription handle.
     */
    template<typename Signal, typename Cb>
    [[nodiscard]] Subscription subscribe(Cb&& cb, bool* replay_dropped = nullptr)
    {
        return Signaling::instance().template subscribe<Signal>(
            std::forward<Cb>(cb),
            replay_dropped);
    }

    /**
     * @brief Creates an observable view through the global signaling dispatcher.
     *
     * @tparam Signal Signal payload type.
     * @return Observable emitting this signal type.
     */
    template<typename Signal>
    Observable<Signal> observe()
    {
        return Signaling::instance().template observe<Signal>();
    }

    /**
     * @brief Reads the last published signal from the global dispatcher.
     *
     * @tparam Signal Signal payload type.
     * @return Last signal if one exists; otherwise `std::nullopt`.
     */
    template<typename Signal>
    [[nodiscard]] std::optional<Signal> last_signal_data()
    {
        return Signaling::last_signal_data<Signal>();
    }
}    // namespace zlibs::utils::signaling
