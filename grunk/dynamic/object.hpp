#pragma once 

#include <sol/sol.hpp>

namespace grunk {

using object = sol::object;

} // namespace grunk

/************
 * Addition *
 ************/

inline grunk::object operator+(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_add"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename R>
inline grunk::object operator+(grunk::object const& l, R const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_add"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename L>
inline grunk::object operator+(L const& l, grunk::object const& r) {
    sol::state_view lua(r.lua_state());
    auto result = lua["grunk"]["__dynamic_add"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

/***************
 * Subtraction *
 ***************/

inline grunk::object operator-(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_sub"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename R>
inline grunk::object operator-(grunk::object const& l, R const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_sub"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename L>
inline grunk::object operator-(L const& l, grunk::object const& r) {
    sol::state_view lua(r.lua_state());
    auto result = lua["grunk"]["__dynamic_sub"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

/******************
 * Multiplication *
 ******************/

inline grunk::object operator*(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_mul"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename R>
inline grunk::object operator*(grunk::object const& l, R const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_mul"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename L>
inline grunk::object operator*(L const& l, grunk::object const& r) {
    sol::state_view lua(r.lua_state());
    auto result = lua["grunk"]["__dynamic_mul"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

/************
 * Division *
 ************/

inline grunk::object operator/(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_div"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename R>
inline grunk::object operator/(grunk::object const& l, R const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_div"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename L>
inline grunk::object operator/(L const& l, grunk::object const& r) {
    sol::state_view lua(r.lua_state());
    auto result = lua["grunk"]["__dynamic_div"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

/*******
 * pow *
 *******/

namespace grunk {

inline grunk::object pow(grunk::object const& base, grunk::object const& exponent) {
    sol::state_view lua(base.lua_state());
    auto result = lua["grunk"]["__dynamic_pow"](base, exponent);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename R>
inline grunk::object pow(grunk::object const& base, R const& exponent) {
    sol::state_view lua(base.lua_state());
    auto result = lua["grunk"]["__dynamic_pow"](base, exponent);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename L>
inline grunk::object pow(L const& base, grunk::object const& exponent) {
    sol::state_view lua(exponent.lua_state());
    auto result = lua["grunk"]["__dynamic_pow"](base, exponent);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

}

/**********
 * modulo *
 **********/

inline grunk::object operator%(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_mod"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename R>
inline grunk::object operator%(grunk::object const& l, R const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_mod"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename L>
inline grunk::object operator%(L const& l, grunk::object const& r) {
    sol::state_view lua(r.lua_state());
    auto result = lua["grunk"]["__dynamic_mod"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

/************
 * Negation *
 ************/

inline grunk::object operator-(grunk::object const& l) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_unm"](l);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

/************
 * Lessthan *
 ************/

inline grunk::object operator<(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_lt"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename R>
inline grunk::object operator<(grunk::object const& l, R const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["__dynamic_lt"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

template <typename L>
inline grunk::object operator<(L const& l, grunk::object const& r) {
    sol::state_view lua(r.lua_state());
    auto result = lua["grunk"]["__dynamic_lt"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}