#pragma once
#include "feature.hpp"
#include "action.hpp"
#include <cmath>

namespace grunk {

namespace details {

    inline lua_State* get_state(Feature<object> const& f) {
        lua_State* lua_state = f.lua_state();
        if (!lua_state && !f.is_placeholder()) {
            lua_state = f.value().lua_state();
        }
        if (!lua_state) {
            throw std::runtime_error("Feature operation: lua state is uninitialized");
        }
        return lua_state;
    }
     
} // namespace details 

/************
 * Addition *
 ************/

template <typename L, typename R>
decltype(auto) operator+(Feature<L> const& l, Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = details::get_state(l);
        } else {
            lua_state = details::get_state(r);
        }
        sol::state_view lua(lua_state);
        function_meta fun = lua["grunk"]["_dynamic_add"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs+rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator+(Feature<L> const& l, R const& r) {
    return l + details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator+(L const& l, Feature<R> const& r) {
    return details::to_feature(l) + r;
}

/***************
 * Subtraction *
 ***************/

template <typename L, typename R>
decltype(auto) operator-(Feature<L> const& l, Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = details::get_state(l);
        } else {
            lua_state = details::get_state(r);
        }
        sol::state_view lua(lua_state);
        function_meta fun = lua["grunk"]["_dynamic_sub"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs-rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator-(Feature<L> const& l, R const& r) {
    return l - details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator-(L const& l, Feature<R> const& r) {
    return details::to_feature(l) - r;
}

/******************
 * Multiplication *
 ******************/

template <typename L, typename R>
decltype(auto) operator*(Feature<L> const& l, Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = details::get_state(l);
        } else {
            lua_state = details::get_state(r);
        }
        sol::state_view lua(lua_state);
        function_meta fun = lua["grunk"]["_dynamic_mul"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs*rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator*(Feature<L> const& l, R const& r) {
    return l * details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator*(L const& l, Feature<R> const& r) {
    return details::to_feature(l) * r;
}

/************
 * Division *
 ************/

template <typename L, typename R>
decltype(auto) operator/(Feature<L> const& l, Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = details::get_state(l);
        } else {
            lua_state = details::get_state(r);
        }
        sol::state_view lua(lua_state);
        function_meta fun = lua["grunk"]["_dynamic_div"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs/rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator/(Feature<L> const& l, R const& r) {
    return l / details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator/(L const& l, Feature<R> const& r) {
    return details::to_feature(l) / r;
}

/*******
 * pow *
 *******/

using std::pow;

template <typename L, typename R>
decltype(auto) pow(Feature<L> const& l, Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = details::get_state(l);
        } else {
            lua_state = details::get_state(r);
        }
        sol::state_view lua(lua_state);
        function_meta fun = lua["grunk"]["_dynamic_pow"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return pow(lhs, rhs); }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) pow(Feature<L> const& l, R const& r) {
    return pow(l, details::to_feature(r));
}

template <typename L, typename R>
decltype(auto) pow(L const& l, Feature<R> const& r) {
    return pow(details::to_feature(l), r);
}

/**********
 * modulo *
 **********/

template <typename L, typename R>
decltype(auto) operator%(Feature<L> const& l, Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = details::get_state(l);
        } else {
            lua_state = details::get_state(r);
        }
        sol::state_view lua(lua_state);
        function_meta fun = lua["grunk"]["_dynamic_mod"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs%rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator%(Feature<L> const& l, R const& r) {
    return l % details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator%(L const& l, Feature<R> const& r) {
    return details::to_feature(l) % r;
}

/************
 * Negation *
 ************/

template <typename L>
Feature<L> operator-(Feature<L> const& l) {
    // if l is a grunk::object, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object>) {
        lua_State* lua_state = details::get_state(l);
        sol::state_view lua(lua_state);
        function_meta fun = lua["grunk"]["_dynamic_unm"];
        return grunk::action(fun, l).output();
    } else {
        // static action
        return grunk::action([](L const& lhs){ return -lhs; }, l).output();
    }
}

}
