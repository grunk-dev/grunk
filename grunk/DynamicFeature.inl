#pragma once 

#include <grunk/DynamicFeature.hpp>
#include <grunk/compute_nodes/Action.hpp>

namespace grunk {

    template <typename... Args>
    DynamicFeature::Feature(std::string const& id, std::string const& typeName, Args const&... args)
     : FeatureBase<reflect::DynamicObject>(id, reflect::make_dynamic(typeName, args...))
     , type_descriptor(reflect::resolve(typeName))
    {}

    template <
        typename T,
        typename
    >
    DynamicFeature::Feature(Feature<T> const& f)
     : Feature(
            details::action(
                f.param().id(),
                [](T const& t){
                     return reflect::DynamicObject(t);
                 },
                f
            ).output()
     )
    {
        type_descriptor = reflect::resolve<T>();
    }

    template <
        typename T, 
        typename
    >
    DynamicFeature::operator Feature<T>() const
    {
        return Feature<T>(param().id(), reflect::cast<T>(this->param().value()));
    }
    
} // namespace grunk
