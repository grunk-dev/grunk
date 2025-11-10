#pragma once 

#include "grunk/core/FeatureBase.hpp"

namespace grunk {

template <typename T>
struct Feature : public FeatureBase<Feature<T>, T> 
{

    Feature(T const& v)
    : FeatureBase<Feature<T>, T>(v)
    {}

    explicit Feature(parametric::param<T> const& p) : FeatureBase<Feature<T>,T>(p) {}

    Feature() : FeatureBase<Feature<T>, T>() {}

};

template <typename T>
Feature<T> feature(T const& v)
{
    return Feature<T>(v);
}

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
    */
    template<typename> constexpr bool is_feature_v = false;

    /**
    * @brief is_feature_v returns true, if the input template argument is a Feature template realization
    * 
    * @tparam T the element type of the Feature
    */
    template<typename T>
    constexpr bool is_feature_v<Feature<T>> = true;


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