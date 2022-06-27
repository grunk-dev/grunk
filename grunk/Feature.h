/**
 * @file Feature.h
 */

#pragma once

#include <parametric/core.hpp>

#include "RuntimeObject.h"

namespace grunk {

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

    T const& Value() const
    {
        return param.value();
    }

    T& AccessValue()
    {
        return param.change_value();
    }

private:

    parametric::param<T> param;
};

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


    // Feature Get(std::string const& memberName) const;

    // RuntimeObject const& Value() const;
    // RuntimeObject& AccessValue();
    
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
