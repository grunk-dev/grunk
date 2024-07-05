#pragma once 

#include <grunk/DynamicFeature.hpp>
#include <grunk/compute_nodes/Action.hpp>

namespace grunk {

    template <typename... Args>
    DynamicFeature::Feature(std::string const& id, std::string const& typeName, Args const&... args)
     : FeatureBase<reflect::DynamicObject>(id, reflect::make_dynamic(typeName, args...))
     , type_descriptor(reflect::resolve(typeName))
    {}
    
} // namespace grunk
