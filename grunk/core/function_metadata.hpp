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

} // namespace grunk