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

};
//TODO: How is this not ambiguous with a Feature<std::string>???
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

    template <typename F, typename... Args>
    decltype(auto) invoke(F const& f, Feature<Args> const&... args) const
    {
        return algorithm(f, *this, args...);
    }

    // Feature Get(std::string const& memberName) const;
    
    // template <typename T>
    // T GetAs(std::string const& memberName) const {
    //     return param.value().GetAs<T>(memberName);
    // }

    // template <typename T> 
    // T cast() const
    // {
    //     return param.value().cast<T>();
    // }


    // bool is_valid() const;

};

template <typename T>
Feature(T&&) -> Feature<T>;

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

};
//TODO: How is this not ambiguous with a Feature<std::string>???
template <typename... Args>
Feature(std::string const&, Args&&...) -> Feature<RuntimeObject>;

using RuntimeFeature = Feature<RuntimeObject>;

} //namespace grunk
