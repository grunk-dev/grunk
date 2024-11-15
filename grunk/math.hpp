#pragma once
#include "action.hpp"
#include <cmath>

namespace grunk {

// define operators

template <typename L, typename R>
Feature<decltype(std::declval<L>() + std::declval<R>())> operator+(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs+rhs; }, l, r).output();
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() - std::declval<R>())> operator-(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs-rhs; }, l, r).output();
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() * std::declval<R>())> operator*(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs*rhs; }, l, r).output();
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() / std::declval<R>())> operator/(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs/rhs; }, l, r).output();
}

using std::pow;
template <typename L, typename R>
Feature<L> pow(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return pow(lhs, rhs); }, l, r).output();
}

template <typename L, typename R>
Feature<decltype(std::declval<L>() % std::declval<R>())> operator%(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs%rhs; }, l, r).output();
}

template <typename L>
Feature<L> operator-(Feature<L> const& l) {
    return grunk::action([](L const& lhs){ return -lhs; }, l).output();
}

}
