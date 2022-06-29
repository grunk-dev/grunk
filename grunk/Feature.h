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
class Feature 
{
public:

    friend class Algorithm;

    template <typename... Args>
    Feature(std::string const& typeName, Args&&... args)
     : param(parametric::new_param(make_rto(typeName, std::forward<Args>(args)...)))
    {}

    Feature(parametric::param<RuntimeObject> const& p);

    Feature Get(std::string const& memberName) const;

    RuntimeObject const& Value() const;
    RuntimeObject& AccessValue();
    
    template <typename T>
    T GetAs(std::string const& memberName) const {
        return param.value().Get(memberName).cast<T>();
    }

    template <typename T> 
    T cast() const
    {
        return param.value().cast<T>();
    }


    bool is_valid() const;

private:
    parametric::param<RuntimeObject> param;
};

} //namespace grunk
