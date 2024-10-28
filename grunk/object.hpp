#pragma once 

#include <sol/sol.hpp>

namespace grunk {

using object = sol::object;

inline object operator+(object const& l, object const& r) {
    sol::state_view lua(l.lua_state());
    return lua["grunk"]["_dynamic_add"](l, r);
    //TODO throw
}

} // anonymous namespace 
