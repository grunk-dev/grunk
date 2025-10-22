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
    std::vector<Parameter> params;
};

inline void register_metadata(
    sol::state_view lua,
    sol::protected_function const& f,
    std::string const& name,
    std::vector<Parameter> const& params)
{
    function_metadata metadata{
        name,
        params
    };
    sol::table registry = lua["grunk"]["registry"];
    registry.set(name, metadata);
    registry.set(f, metadata);
}

inline function_metadata get_metadata(
sol::protected_function const& f)
{
    sol::state_view lua(f.lua_state());
    sol::table registry = lua["grunk"]["registry"];

    sol::object metadata_obj = registry[f];
    if (!metadata_obj.is<function_metadata>()) {
        throw std::runtime_error("Function metadata not found in registry.");
    }
    return metadata_obj.as<function_metadata>();
}

} // namespace grunk
