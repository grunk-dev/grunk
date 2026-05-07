// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/core/FeatureBase.hpp"

namespace grunk {

/**
 * @ingroup core
 * @brief The Feature class template is the main interface for users to create and manipulate features in grunk. It is a thin wrapper around parametric::param that provides some additional functionality and syntactic sugar.
 * 
 * @tparam T the type of the feature value, e.g. double, std::string or even a user defined type
*/
template <typename T>
struct Feature : public FeatureBase<Feature<T>, T> 
{

    /**
     * @brief constructs a feature with the given value. The value can be a literal, a variable or the output of an action. 
     * 
     * There are three kinds of features, disambiguated by the existence of a name or a value.
     * 1. A feature with a value and no name is often called a constanct, because it cannot be retrieved by name. It is not listed in the parameters block of a recipe
     * 2. A feature with a name and a value is often called a variable, because it can be retrieved by name and thus manipulated. It is listed in the parameters block of a recipe
     * 3. A feature with a name but no value is often called a placeholder, because it can be retrieved by name but cannot be evaluated. It is listed in the parameters block of a recipe and is used to mark missing values that need to be filled in by the user or by other recipes.
     *
     * @param v the value of the feature
     */
    Feature(T const& v)
    : FeatureBase<Feature<T>, T>(v) 
    {}

    /**
     * @brief constructs a feature from a parametric::param. This is used internally for cloning and other operations that manipulate the underlying DAG.
     * 
     * @param p the parametric::param to construct the feature from
     */
    explicit Feature(parametric::param<T> const& p) : FeatureBase<Feature<T>,T>(p) {}

    /**
     * @brief constructs an empty feature.
     * 
     */
    Feature() : FeatureBase<Feature<T>, T>() {}

};

/**
 * @brief feature is a factory function to construct a Feature
 * 
 * @tparam T the type of the feature value, e.g. double, std::string or even a user defined type
 * @param v the value of the feature
 * @return Feature<T> a feature with the given value
 *
 * @ingroup core
 */
template <typename T>
Feature<T> feature(T const& v)
{
    return Feature<T>(v);
}

/**
 * @brief feature is a factory function to construct an empty (placeholder) Feature.
 * 
 * @tparam T the type of the feature value, e.g. double, std::string or even a user defined type
 * @return Feature<T> an empty feature
 * 
 * @ingroup core
 */
template <typename T>
Feature<T> feature()
{
    return Feature<T>();
}

namespace details {

    /**
    * @brief is_feature_v returns false if the input type is not a Feature template realization
    * 
    * @tparam typename any ol' type
    *
    * @ingroup advanced_core
    */
    template<typename> constexpr bool is_feature_v = false;

    /**
    * @brief is_feature_v returns true, if the input template argument is a Feature template realization
    * 
    * @tparam T the element type of the Feature
    *
    * @ingroup advanced_core
    */
    template<typename T>
    constexpr bool is_feature_v<Feature<T>> = true;


    /**
     * @brief to_feature is a helper function to convert an input to a feature. If the input is already a feature, it is returned as is. Otherwise, it is wrapped in an unnamed/anonymous feature.
     * 
     * @tparam T the type of the input value
     * @param arg the input value
     * @return auto a feature representing the input value
     * 
     * @ingroup advanced_core
     */
    template <typename T>
    auto to_feature(T const& arg)
    {
        if constexpr (details::is_feature_v<T>){
            return arg;
        } else {
            return grunk::feature<T>(arg);
        }
    };

} // namespace details

} // namespace grunk