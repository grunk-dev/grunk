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
    std::string m_name;
    std::vector<Parameter> m_params;
};

} // namespace grunk