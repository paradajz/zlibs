/*
 * Copyright (c) 2022 Michał Adamczyk
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 *
 * Derived from original work by Michał Adamczyk published under MIT License.
 * Modified for integration into the zlibs project.
 */

#pragma once

#include "zlibs/utils/misc/meta.h"
#include "zlibs/utils/misc/noncopyable.h"

#include <cstddef>
#include <tuple>
#include <variant>

namespace zlibs::utils::state_machine
{
    /**
     * @brief Action that transitions a machine to a target state.
     *
     * @tparam State Destination state type.
     */
    template<typename State>
    struct Transition
    {
        /**
         * @brief Executes the transition on a state machine.
         *
         * @tparam Machine State machine type.
         * @param machine Machine instance.
         */
        template<typename Machine>
        void execute(Machine& machine)
        {
            machine.template transition_to<State>();
        }
    };

    /**
     * @brief No-op state machine action.
     */
    struct Nothing
    {
        /**
         * @brief Executes no action.
         *
         * @tparam Machine State machine type.
         * @param machine Unused machine instance.
         */
        template<typename Machine>
        void execute([[maybe_unused]] Machine& machine)
        {}
    };

    /**
     * @brief Variant action wrapper that executes one of the provided action types.
     *
     * @tparam Actions Supported action types.
     */
    template<typename... Actions>
    class OneOf
    {
        public:
        /**
         * @brief Constructs the action wrapper from one action instance.
         *
         * @tparam T Action type compatible with the variant.
         * @param arg Action value.
         */
        template<typename T>
            requires(std::is_constructible_v<std::variant<Actions...>, T>)
        OneOf(T&& arg)
            : _options(std::forward<T>(arg))
        {}

        /**
         * @brief Executes the selected action.
         *
         * @tparam Machine State machine type.
         * @param machine Machine instance.
         */
        template<typename Machine>
        void execute(Machine& machine)
        {
            std::visit([&machine](auto& action)
                       {
                           action.execute(machine);
                       },
                       _options);
        }

        private:
        std::variant<Actions...> _options;
    };

    /**
     * @brief Optional action wrapper, allowing either an action or `Nothing`.
     *
     * @tparam Actions Supported action types.
     */
    template<typename... Actions>
    struct Maybe : public OneOf<Actions..., Nothing>
    {
        using OneOf<Actions..., Nothing>::OneOf;
    };

    /**
     * @brief Tag base class marking the initial state of a machine.
     */
    struct InitStateTag
    {
    };

    /**
     * @brief Tag base class marking the error state of a machine.
     */
    struct ErrorStateTag
    {
    };

    /**
     * @brief Checks whether a state type inherits from `InitStateTag`.
     *
     * @tparam T State type.
     */
    template<typename T>
    concept IsInitState = std::derived_from<T, InitStateTag>;

    /**
     * @brief Checks whether a state type inherits from `ErrorStateTag`.
     *
     * @tparam T State type.
     */
    template<typename T>
    concept IsErrorState = std::derived_from<T, ErrorStateTag>;

    /**
     * @brief Type trait wrapper for `IsInitState`.
     *
     * @tparam T State type.
     */
    template<typename T>
    struct IsInit : std::bool_constant<std::derived_from<T, InitStateTag>>
    {
    };

    /**
     * @brief Type trait wrapper for `IsErrorState`.
     *
     * @tparam T State type.
     */
    template<typename T>
    struct IsError : std::bool_constant<std::derived_from<T, ErrorStateTag>>
    {
    };

    /**
     * @brief Static-typed finite state machine.
     *
     * Exactly one state must inherit `InitStateTag`, and exactly one state must
     * inherit `ErrorStateTag`.
     *
     * @tparam States State types stored by the machine.
     */
    template<typename... States>
        requires zlibs::utils::misc::AllUnique<States...>
    class StateMachine : public zlibs::utils::misc::NonCopyableOrMovable
    {
        static_assert(sizeof...(States) >= 2, "At least two states need to be provided to the state machine");
        static_assert((IsInitState<States> + ...) == 1, "Exactly one state must inherit from InitStateTag");
        static_assert((IsErrorState<States> + ...) == 1, "Exactly one state must inherit from ErrorStateTag");

        /** @brief State type marked as the machine entry state. */
        using InitState = typename zlibs::utils::misc::FindFirst<IsInit, States...>::type;

        /** @brief State type marked as the machine fallback/error state. */
        using ErrorState = typename zlibs::utils::misc::FindFirst<IsError, States...>::type;

        public:
        StateMachine() = default;

        /**
         * @brief Initializes the machine and enters init state.
         *
         * On failure, transitions to error state.
         *
         * @return `true` when init state enter succeeds, otherwise `false`.
         */
        bool init()
        {
            _current_state = &std::get<InitState>(_states);

            if (!enter(_current_state))
            {
                _current_state = &std::get<ErrorState>(_states);
                enter(_current_state);
                return false;
            }

            _initialized = true;

            return true;
        }

        /**
         * @brief Dispatches an event to the current state if supported.
         *
         * @tparam Event Event type.
         * @param event Event payload.
         */
        template<typename Event>
        void handle(const Event& event)
        {
            if (!_initialized)
            {
                return;
            }

            auto pass_event_to_state = [this, &event](auto state_ptr)
            {
                if constexpr (requires { state_ptr->handle(event); })
                {
                    state_ptr->handle(event).execute(*this);
                }
            };

            std::visit(pass_event_to_state, _current_state);
        }

        /**
         * @brief Transitions from current state to target state.
         *
         * If exit/enter fails, machine enters error state.
         *
         * @tparam State Destination state type.
         * @return `true` when transition succeeds, otherwise `false`.
         */
        template<typename State>
            requires misc::PackContains<State, States...>
        bool transition_to()
        {
            if (!_initialized)
            {
                return false;
            }

            if (!exit(_current_state))
            {
                _current_state = &std::get<ErrorState>(_states);
                enter(_current_state);
                return false;
            }

            _current_state = &std::get<State>(_states);

            if (!enter(_current_state))
            {
                _current_state = &std::get<ErrorState>(_states);
                enter(_current_state);
                return false;
            }

            return true;
        }

        /**
         * @brief Checks whether machine is currently in a specific state.
         *
         * @tparam State State type to check.
         * @return `true` if current state matches `State`, otherwise `false`.
         */
        template<typename State>
            requires misc::PackContains<State, States...>
        bool is_in_state() const
        {
            return std::holds_alternative<State*>(_current_state);
        }

        /**
         * @brief Returns compile-time index of a state within `States...`.
         *
         * @tparam State State type.
         * @return Zero-based index in the state pack.
         */
        template<typename State>
            requires misc::PackContains<State, States...>
        static consteval size_t state_index()
        {
            size_t index = 0;
            size_t i     = 0;

            [[maybe_unused]] const bool FOUND =
                ((std::is_same_v<State, States> ? (index = i, true) : (++i, false)) || ...);

            return index;
        }

        /**
         * @brief Returns current runtime state index.
         *
         * @return Zero-based index matching variant order.
         */
        size_t current_state_index() const
        {
            return _current_state.index();
        }

        private:
        /** @brief Variant holding a pointer to the currently active state object. */
        using StateVariantPtr = std::variant<States*...>;

        /** @brief Tuple owning all state instances managed by the machine. */
        using StateTuple = std::tuple<States...>;

        StateTuple      _states;
        StateVariantPtr _current_state = {};
        bool            _initialized   = false;

        /**
         * @brief Calls `enter()` on the pointed state.
         *
         * @param state_ptr Variant holding pointer to state instance.
         *
         * @return `true` when state enter succeeds, otherwise `false`.
         */
        bool enter(const StateVariantPtr& state_ptr)
        {
            auto enter_state = [](auto state_ptr)
            {
                return state_ptr->enter();
            };

            return std::visit(enter_state, state_ptr);
        }

        /**
         * @brief Calls `exit()` on the pointed state.
         *
         * @param state_ptr Variant holding pointer to state instance.
         *
         * @return `true` when state exit succeeds, otherwise `false`.
         */
        bool exit(const StateVariantPtr& state_ptr)
        {
            auto exit_state = [](auto state_ptr)
            {
                return state_ptr->exit();
            };

            return std::visit(exit_state, state_ptr);
        }
    };
}    // namespace zlibs::utils::state_machine
