#pragma once
#include "feature.hpp"
#include "action.hpp"
#include <cmath>

namespace grunk {

/************
 * Addition *
 ************/

template <typename L, typename R>
Feature<decltype(std::declval<L>() + std::declval<R>())> operator+(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs+rhs; }, l, r).output();
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() + std::declval<R>())> operator+(Feature<L> const& l, R const& r) {
    return l + details::to_feature(r);
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() + std::declval<R>())> operator+(L const& l, Feature<R> const& r) {
    return details::to_feature(l) + r;
}

/***************
 * Subtraction *
 ***************/

template <typename L, typename R>
Feature<decltype(std::declval<L>() - std::declval<R>())> operator-(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs-rhs; }, l, r).output();
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() - std::declval<R>())> operator-(Feature<L> const& l, R const& r) {
    return l - details::to_feature(r);
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() - std::declval<R>())> operator-(L const& l, Feature<R> const& r) {
    return details::to_feature(l) - r;
}

/******************
 * Multiplication *
 ******************/

template <typename L, typename R>
Feature<decltype(std::declval<L>() * std::declval<R>())> operator*(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs*rhs; }, l, r).output();
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() * std::declval<R>())> operator*(Feature<L> const& l, R const& r) {
    return l * details::to_feature(r);
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() * std::declval<R>())> operator*(L const& l, Feature<R> const& r) {
    return details::to_feature(l) * r;
}

/************
 * Division *
 ************/

template <typename L, typename R>
Feature<decltype(std::declval<L>() / std::declval<R>())> operator/(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs/rhs; }, l, r).output();
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() / std::declval<R>())> operator/(Feature<L> const& l, R const& r) {
    return l / details::to_feature(r);
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() / std::declval<R>())> operator/(L const& l, Feature<R> const& r) {
    return details::to_feature(l) / r;
}

/*******
 * pow *
 *******/

using std::pow;

template <typename L, typename R>
using pow_result_t = std::invoke_result_t<decltype(&pow<L const&, R const&>), L, R>;

template <typename L, typename R>
auto pow(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return pow(lhs, rhs); }, l, r).output();
}

template <typename L, typename R>
auto pow(Feature<L> const& l, R const& r) {
    return pow(l, details::to_feature(r));
}

template <typename L, typename R>
auto pow(L const& l, Feature<R> const& r) {
    return pow(details::to_feature(l), r);
}

/**********
 * modulo *
 **********/

template <typename L, typename R>
Feature<decltype(std::declval<L>() % std::declval<R>())> operator%(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs%rhs; }, l, r).output();
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() % std::declval<R>())> operator%(Feature<L> const& l, R const& r) {
    return l % details::to_feature(r);
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() % std::declval<R>())> operator%(L const& l, Feature<R> const& r) {
    return details::to_feature(l) % r;
}

/************
 * Negation *
 ************/

template <typename L>
Feature<L> operator-(Feature<L> const& l) {
    return grunk::action([](L const& lhs){ return -lhs; }, l).output();
}

}
