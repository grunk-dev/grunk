// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

/**
 * @file Feature.hpp
 *
 * Declaration and Definition of the Feature class.
 *
 * @defgroup core
 * @defgroup advanced
 */

#pragma once 

#ifndef GRUNK_WITH_DYNAMIC

/************
 * Addition *
 ************/

template <typename L, typename R>
decltype(auto) operator+(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs+rhs; }, l, r).output();
}

template <typename L, typename R>
decltype(auto) operator+(grunk::Feature<L> const& l, R const& r) {
    return l + details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator+(L const& l, grunk::Feature<R> const& r) {
    return details::to_feature(l) + r;
}

/***************
 * Subtraction *
 ***************/

template <typename L, typename R>
decltype(auto) operator-(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs-rhs; }, l, r).output();
}

template <typename L, typename R>
decltype(auto) operator-(grunk::Feature<L> const& l, R const& r) {
    return l - details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator-(L const& l, grunk::Feature<R> const& r) {
    return details::to_feature(l) - r;
}

/******************
 * Multiplication *
 ******************/

template <typename L, typename R>
decltype(auto) operator*(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs*rhs; }, l, r).output();
}

template <typename L, typename R>
decltype(auto) operator*(grunk::Feature<L> const& l, R const& r) {
    return l * details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator*(L const& l, grunk::Feature<R> const& r) {
    return details::to_feature(l) * r;
}

/************
 * Division *
 ************/

template <typename L, typename R>
decltype(auto) operator/(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs/rhs; }, l, r).output();
}

template <typename L, typename R>
decltype(auto) operator/(grunk::Feature<L> const& l, R const& r) {
    return l / details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator/(L const& l, grunk::Feature<R> const& r) {
    return details::to_feature(l) / r;
}

/*******
 * pow *
 *******/

 namespace grunk {

using std::pow;

template <typename L, typename R>
decltype(auto) pow(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return pow(lhs, rhs); }, l, r).output();
}

template <typename L, typename R>
decltype(auto) pow(grunk::Feature<L> const& l, R const& r) {
    return pow(l, details::to_feature(r));
}

template <typename L, typename R>
decltype(auto) pow(L const& l, grunk::Feature<R> const& r) {
    return pow(details::to_feature(l), r);
}

}  // namespace grunk

/**********
 * modulo *
 **********/

template <typename L, typename R>
decltype(auto) operator%(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs%rhs; }, l, r).output();
}

template <typename L, typename R>
decltype(auto) operator%(grunk::Feature<L> const& l, R const& r) {
    return l % details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator%(L const& l, grunk::Feature<R> const& r) {
    return details::to_feature(l) % r;
}

/************
 * Negation *
 ************/

template <typename L>
grunk::Feature<L> operator-(grunk::Feature<L> const& l) {
    return grunk::action([](L const& lhs){ return -lhs; }, l).output();
}

#endif // not GRUNK_WITH_DYNAMIC