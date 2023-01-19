#pragma once 

#include "DynamicFeature.hpp"
#include "DynamicAction.hpp"

namespace grunk {

    template <typename... Args>
    ActionPtr<reflect::DynamicFunction> action(std::string const& id, std::string const& name, Feature<Args> const&... args);

    template <typename... Args>
    DynamicFeature::Feature(std::string const& id, const char* typeName, Args const&... args)
     : FeatureBase<reflect::DynamicObject>(id, reflect::make_dynamic(typeName, args...))
     , type_descriptor(reflect::resolve(typeName))
    {}

    template <typename... Args>
    DynamicFeature::Feature(std::string const& id, const char* typeName, Feature<Args> const&... args)
     : Feature(
        // std::move(
            action(
                id, typeName, args...
            )->output()
        // )
     )
    {}

    template <
        typename T,
        typename
    >
    DynamicFeature::Feature(Feature<T> const& f)
     : Feature(
            action(
                f.param().id(),
                [](T const& t){
                     return reflect::DynamicObject(t);
                 },
                f
            )->output()
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

    template <typename... Args>
    decltype(auto) DynamicFeature::invoke(std::string const& memberFunName, Feature<Args> const&... args) const
    {
        return action(
            param().id() + "::" + memberFunName, // TODO: How would we name this by default?
            type_descriptor->get_name() + "::" + memberFunName,
            *this,
            args...
        );
    }

} // namespace grunk
