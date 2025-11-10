#pragma once
#include "DynamicFeature.hpp"
#include "grunk/core/ActionStatic.hpp"
#include "ActionDynamic.hpp"
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

} // namespace grunk

#ifdef GRUNK_WITH_DYNAMIC

/************
 * Addition *
 ************/

template <typename L, typename R>
decltype(auto) operator+(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = grunk::details::get_state(l);
        } else {
            lua_state = grunk::details::get_state(r);
        }
        sol::state_view lua(lua_state);
        grunk::function_meta fun = lua["grunk"]["_dynamic_add"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs+rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator+(grunk::Feature<L> const& l, R const& r) {
    return l + grunk::details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator+(L const& l, grunk::Feature<R> const& r) {
    return grunk::details::to_feature(l) + r;
}

/***************
 * Subtraction *
 ***************/

template <typename L, typename R>
decltype(auto) operator-(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = grunk::details::get_state(l);
        } else {
            lua_state = grunk::details::get_state(r);
        }
        sol::state_view lua(lua_state);
        grunk::function_meta fun = lua["grunk"]["_dynamic_sub"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs-rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator-(grunk::Feature<L> const& l, R const& r) {
    return l - grunk::details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator-(L const& l, grunk::Feature<R> const& r) {
    return grunk::details::to_feature(l) - r;
}

/******************
 * Multiplication *
 ******************/

template <typename L, typename R>
decltype(auto) operator*(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = grunk::details::get_state(l);
        } else {
            lua_state = grunk::details::get_state(r);
        }
        sol::state_view lua(lua_state);
        grunk::function_meta fun = lua["grunk"]["_dynamic_mul"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs*rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator*(grunk::Feature<L> const& l, R const& r) {
    return l * grunk::details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator*(L const& l, grunk::Feature<R> const& r) {
    return grunk::details::to_feature(l) * r;
}

/************
 * Division *
 ************/

template <typename L, typename R>
decltype(auto) operator/(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = grunk::details::get_state(l);
        } else {
            lua_state = grunk::details::get_state(r);
        }
        sol::state_view lua(lua_state);
        grunk::function_meta fun = lua["grunk"]["_dynamic_div"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs/rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator/(grunk::Feature<L> const& l, R const& r) {
    return l / grunk::details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator/(L const& l, grunk::Feature<R> const& r) {
    return grunk::details::to_feature(l) / r;
}

/*******
 * pow *
 *******/

namespace grunk {

using std::pow;

template <typename L, typename R>
decltype(auto) pow(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = grunk::details::get_state(l);
        } else {
            lua_state = grunk::details::get_state(r);
        }
        sol::state_view lua(lua_state);
        grunk::function_meta fun = lua["grunk"]["_dynamic_pow"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return pow(lhs, rhs); }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) pow(grunk::Feature<L> const& l, R const& r) {
    return pow(l, grunk::details::to_feature(r));
}

template <typename L, typename R>
decltype(auto) pow(L const& l, grunk::Feature<R> const& r) {
    return pow(grunk::details::to_feature(l), r);
}

} // namespace grunk

/**********
 * modulo *
 **********/

template <typename L, typename R>
decltype(auto) operator%(grunk::Feature<L> const& l, grunk::Feature<R> const& r) {
    // if either l or r are grunk::objects, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object> || std::is_same_v<R, grunk::object>) {
        lua_State* lua_state = nullptr;
        if constexpr (std::is_same_v<L, grunk::object>) {
            lua_state = grunk::details::get_state(l);
        } else {
            lua_state = grunk::details::get_state(r);
        }
        sol::state_view lua(lua_state);
        grunk::function_meta fun = lua["grunk"]["_dynamic_mod"];
        return grunk::action(fun, l, r).output();
    } else {
        // static action
        return grunk::action([](L const& lhs, R const& rhs){ return lhs%rhs; }, l, r).output();
    }
}

template <typename L, typename R>
decltype(auto) operator%(grunk::Feature<L> const& l, R const& r) {
    return l % grunk::details::to_feature(r);
}

template <typename L, typename R>
decltype(auto) operator%(L const& l, grunk::Feature<R> const& r) {
    return grunk::details::to_feature(l) % r;
}

/************
 * Negation *
 ************/

template <typename L>
grunk::Feature<L> operator-(grunk::Feature<L> const& l) {
    // if l is a grunk::object, the result will be a dynamic action
    // this distinction is necessary to allow deserialization of dynamic actions
    if constexpr (std::is_same_v<L, grunk::object>) {
        lua_State* lua_state = grunk::details::get_state(l);
        sol::state_view lua(lua_state);
        grunk::function_meta fun = lua["grunk"]["_dynamic_unm"];
        return grunk::action(fun, l).output();
    } else {
        // static action
        return grunk::action([](L const& lhs){ return -lhs; }, l).output();
    }
}

#endif // GRUNK_WITH_DYNAMIC