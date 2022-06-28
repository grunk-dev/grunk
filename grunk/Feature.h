/**
 * @file Feature.h
 */

#pragma once

#include <parametric/core.hpp>

#include "RuntimeObject.h"

namespace grunk {

//forward declarations
template <typename T>
class Feature;

template <typename F, typename... Args>
class Algorithm;

template <typename F, typename... Args>
parametric::compute_node_ptr<Algorithm<F, Args...>> algorithm(F const& fun, Feature<Args> const&... args);

/**
 * @brief This class does ...
 *
 * A more detailed description of this class can be found here.
 */
template <typename T>
class FeatureBase {
public:

    template <typename F, typename... Args>
    friend class Algorithm;

    FeatureBase(T&& t)
     : param(parametric::new_param(std::forward<T>(t)))
    {}

    FeatureBase(parametric::param<T>&& p)
     : param(p)
    {}

    bool is_valid() const
    {
        return param.is_valid();
    }

    T const& value() const
    {
        return param.value();
    }

    T& access_value()
    {
        return param.change_value();
    }

protected:

    parametric::param<T> param;
};

template <>
class Feature<RuntimeObject> : public FeatureBase<RuntimeObject>
{
public:

    template <typename... Args>
    Feature(std::string const& typeName, Args&&... args)
     : FeatureBase<RuntimeObject>(make_rto(typeName, std::forward<Args>(args)...))
    {}

    Feature(parametric::param<RuntimeObject>&& p)
     : FeatureBase<RuntimeObject>(std::forward<parametric::param<RuntimeObject>>(p))
    {}

    Feature(RuntimeObject&& o)
     : FeatureBase(std::forward<RuntimeObject>(o))
    {}

    template <typename T, typename = std::enable_if_t<!std::is_same_v<T, RuntimeObject>>>
    operator Feature<T>() const
    {
        return Feature<T>(this->param.value().cast<T>());
    }

    decltype(auto) get(std::string const& memberName) const
    {
        return algorithm(
            [=](RuntimeObject const& wrapped){
                return wrapped.Get(memberName);
            },
            *this
        );
    }

    template <typename... Args>
    decltype(auto) invoke(std::string const& memberFunName, Feature<Args> const&... args) const
    {
        return algorithm(
            [=](auto const& wrapped, auto const&... arguments){
                return wrapped.Invoke(memberFunName, arguments...);
            },
            *this,
            args...
        );
    }

};
template <typename... Args>
Feature(std::string const&, Args&&...) -> Feature<RuntimeObject>;

using RuntimeFeature = Feature<RuntimeObject>;

template <typename T>
class Feature : public FeatureBase<T>
{
public:

    Feature(T&& t)
     : FeatureBase<T>(std::forward<T>(t))
    {}

    Feature(parametric::param<T>&& p)
     : FeatureBase<T>(std::forward<parametric::param<T>>(p))
    {}

    template <typename = std::enable_if_t<!std::is_same_v<RuntimeObject, T>>>
    operator Feature<RuntimeObject>() const
    {
        return Feature<RuntimeObject>(RuntimeObject(this->param.value()));
    }

    template <typename MemberPtr>
    decltype(auto) get(MemberPtr ptr) const
    {
        return algorithm(
            [=](auto const& wrapped){ 
                return wrapped.*ptr; 
            }, 
            *this
        );
    }

    template <typename MemberFunPtr, typename... Args>
    decltype(auto) invoke(MemberFunPtr funPtr, Feature<Args> const&... args) const
    {
        return algorithm(
            [=](auto const& wrapped, auto const&... arguments){
                return (wrapped.*funPtr)(arguments...);
            },
            *this,
            args...
        );
    }

    // template <typename T>
    // T GetAs(std::string const& memberName) const {
    //     return param.value().GetAs<T>(memberName);
    // }

    // template <typename T> 
    // T cast() const
    // {
    //     return param.value().cast<T>();
    // }
};

template <typename T>
Feature(T&&) -> Feature<T>;

} //namespace grunk

#include "Algorithm.h"
