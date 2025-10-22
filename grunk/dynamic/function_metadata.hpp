#pragma once 

#include "object.hpp"

namespace grunk {

struct Parameter 
{
    std::optional<std::string> name {std::nullopt};
    //TODO: type information?
    //TODO: default value?
    bool is_const_reference {false};
};

struct function_metadata
{
    std::string name;
    std::optional<std::vector<Parameter>> params;
};

inline void register_metadata(
    sol::reference const& f,
    std::string const& name,
    std::vector<Parameter> const& params)
{
    function_metadata metadata{
        name,
        params
    };
    sol::state_view lua(f.lua_state());
    sol::table registry = lua["grunk"]["registry"];
    registry.set(name, metadata);

    // Push f and get its pointer
    f.push();
    const void* func_ptr = lua_topointer(f.lua_state(), -1);
    lua_pop(f.lua_state(), 1);

    registry.set(func_ptr, metadata);
}

inline std::optional<function_metadata> get_metadata(
    sol::reference const& f
)
{
    sol::state_view lua(f.lua_state());
    sol::table registry = lua["grunk"]["registry"];

    // Push f and get its pointer
    f.push();
    const void* func_ptr = lua_topointer(f.lua_state(), -1);
    lua_pop(f.lua_state(), 1);

    sol::object metadata_obj = registry[func_ptr];
    if (!metadata_obj.is<function_metadata>()) {
        return std::nullopt;
    }
    return metadata_obj.as<function_metadata>();
}

} // namespace grunk
