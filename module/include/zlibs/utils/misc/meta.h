/*
 * Copyright (c) 2026 Igor Petrovic
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <concepts>
#include <type_traits>

namespace zlibs::utils::misc
{
    /**
     * @brief Checks whether `T` appears exactly once in a parameter pack.
     *
     * @tparam T Type to check.
     * @tparam Ts Pack of types.
     */
    template<typename T, typename... Ts>
    concept UniqueInPack = (std::same_as<T, Ts> + ...) == 1;

    /**
     * @brief Checks whether all types in a parameter pack are unique.
     *
     * @tparam T Pack of types.
     */
    template<typename... T>
    concept AllUnique = (UniqueInPack<T, T...> && ...);

    /**
     * @brief Checks whether `T` exists in a type pack.
     *
     * @tparam T Type to search.
     * @tparam Ts Pack of types.
     */
    template<typename T, typename... Ts>
    concept PackContains = (std::is_same_v<T, Ts> || ...);

    /**
     * @brief Finds the first type in a pack satisfying a unary predicate.
     *        Yields `void` if no match is found.
     *
     * @tparam Pred Unary predicate template.
     * @tparam Ts   Candidate types.
     */
    template<template<typename> typename Pred, typename... Ts>
    struct FindFirst : std::type_identity<void>
    {
    };

    /**
     * @brief Recursive specialization of `FindFirst` that tests the head type `T`.
     *
     * Inherits from `std::type_identity<T>` if `Pred<T>::value` is true,
     * otherwise recurses into `FindFirst<Pred, Ts...>`.
     *
     * @tparam Pred Unary predicate template.
     * @tparam T    Current head type being tested.
     * @tparam Ts   Remaining candidate types.
     */
    template<template<typename> typename Pred, typename T, typename... Ts>
    struct FindFirst<Pred, T, Ts...>
        : std::conditional_t<Pred<T>::value, std::type_identity<T>, FindFirst<Pred, Ts...>>
    {
    };
}    // namespace zlibs::utils::misc
