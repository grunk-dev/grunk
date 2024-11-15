#pragma once 

#include <sol/sol.hpp>

namespace grunk {

using object = sol::object;

inline grunk::object operator+(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["_dynamic_add"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

inline grunk::object operator-(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["_dynamic_sub"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

inline grunk::object operator*(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["_dynamic_mul"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

inline grunk::object operator/(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["_dynamic_div"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}


inline grunk::object pow(grunk::object const& base, grunk::object const& exponent) {
    sol::state_view lua(base.lua_state());
    auto result = lua["grunk"]["_dynamic_pow"](base, exponent);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}

inline grunk::object operator%(grunk::object const& l, grunk::object const& r) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["_dynamic_mod"](l, r);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}


inline grunk::object operator-(grunk::object const& l) {
    sol::state_view lua(l.lua_state());
    auto result = lua["grunk"]["_dynamic_unm"](l);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error("Lua error: " + std::string(err.what()));
    }
    return result;
}


} // namespace grunk
