/**
 * @file RuntimeFeature.h
 */

#pragma once

#include <grunk/core/Feature.h>

namespace grunk {

template <>
class Feature<RuntimeObject> : public FeatureBase<RuntimeObject>
{
public:

    template <typename... Args>
    Feature(const char* typeName, Args&&... args)
     : FeatureBase<RuntimeObject>(make_rto(typeName, std::forward<Args>(args)...))
    {}

    Feature(parametric::param<RuntimeObject>&& p)
     : FeatureBase<RuntimeObject>(std::forward<parametric::param<RuntimeObject>>(p))
    {}

    explicit Feature(RuntimeObject&& o)
     : FeatureBase(std::forward<RuntimeObject>(o))
    {}

    template <typename T,
              typename = std::enable_if_t<!std::is_same_v<RuntimeObject, T>>
    >
    Feature(Feature<T> const& f)
     : Feature(RuntimeObject(f.value()))
    {}

    template <typename T, typename = std::enable_if_t<!std::is_same_v<T, RuntimeObject>>>
    operator Feature<T>() const
    {
        return Feature<T>(this->param.value().cast<T>());
    }

    decltype(auto) get(std::string const& memberName) const
    {
        return eval(
            [=](RuntimeObject const& wrapped){
                return wrapped.get(memberName);
            },
            *this
        );
    }

    template <typename... Args>
    decltype(auto) invoke(std::string const& memberFunName, Feature<Args> const&... args) const
    {
        return eval(
            [=](RuntimeObject const& wrapped, auto const&... arguments){
                return wrapped.invoke(memberFunName, arguments...);
            },
            *this,
            args...
        );
    }

};
template <typename... Args>
Feature(std::string const&, Args&&...) -> Feature<RuntimeObject>;

using RuntimeFeature = Feature<RuntimeObject>;

}