/**
 * @file RuntimeFeature.h
 *
 * This file contains the template specialization of Feature for Reflect::DynamicObjects
 */

#pragma once

#include <grunk/core/Feature.h>

namespace grunk {

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

} // namespace details

/**
 * @brief template specialization of Feature for Reflect::DynamicObjects
 *
 * In addition to the dependency management provided by the parametric library,
 * this class provides an interface to retrieve data members and invoke member
 * functions on the wrapped object and registering this action in the feature 
 * tree.
 * 
 * @ingroup dynamic  
 */
template <>
class Feature<Reflect::DynamicObject> : public FeatureBase<Reflect::DynamicObject>
{
public:

    /**
     * @brief Construct a new RuntimeFeature given the string representation
     * of a reflected type and constructor arguments.
     *
     * This constructs a runtime object given constructor arguments and wraps 
     * it in a feature instance. See also make_rto.
     * 
     * @tparam Args The constructor arguments
     * @param typeName The string representation of the reflected type
     * @param args The constructor arguments
     */
    template <
        typename... Args,
        typename = std::enable_if_t<!(details::is_feature_v<std::decay_t<Args>> || ...)>
    >
    Feature(const char* typeName, Args&&... args)
     : FeatureBase<Reflect::DynamicObject>(Reflect::make_dynamic(typeName, std::forward<Args>(args)...))
    {}


    /**
     * @brief Construct a new RuntimeFeature object given constructor arguments
     * Feature<Args>..., where T is constructable from Args...
     *
     * Example: 
     * @code
     *
     * struct Foo {
     *      Foo(double, int){}
     * };
     *
     * Feature<double> x(4.2);
     * Feature<int> y(13);
     *
     * RuntimeFeature z("Foo", x, y);
     * \endcode
     *
     * Whenever one of the constructor arguments x or y changes, the Feature z will 
     * be marked for lazy reconstruction.
     *
     * Caveat: We cannot pass RuntimeFeatures as input arguments, because this would
     * not allow grunk to deduce the correct Constructor arguments to select
     * the correct constructor.
     * 
     * @tparam Args Constructor argument types for the type to be constructed
     * @param typeName The string representation of the reflected type
     * @param args input Features for the constructor for the type to b constructed 
     */
    template <typename... Args>
    Feature(const char* typeName, Feature<Args> const&... args)
     : Feature(
        eval(
            [=](Args const&... in){
                return Reflect::make_dynamic(typeName, in...);
            },
            args...
        )->get()
     )
    {}

    /**
     * @brief Construct a new RuntimeFeature given a parametric::param<T>
     * 
     * @param p The parametric::param<T> to be wrapped in a Feature
     */
    Feature(parametric::param<Reflect::DynamicObject>&& p)
     : FeatureBase<Reflect::DynamicObject>(std::forward<parametric::param<Reflect::DynamicObject>>(p))
    {}

    /**
     * @brief Construct a new RuntimeFeature given an Reflect::DynamicObject
     * 
     * @param o The input Reflect::DynamicObject
     */
    explicit Feature(Reflect::DynamicObject&& o)
     : FeatureBase(std::forward<Reflect::DynamicObject>(o))
    {}

    /**
     * @brief Converting constructor from a Feature<T>, where T is not
     * a Reflect::DynamicObject
     * 
     * @tparam T The type wrapped by the incoming Feature<T>
     * @param f The input feature to be converted to a RuntimeFeature
     */
    template <typename T,
              typename = std::enable_if_t<!std::is_same_v<Reflect::DynamicObject, T>>
    >
    Feature(Feature<T> const& f)
     : Feature(Reflect::DynamicObject(f.value()))
    {}

    /**
     * @brief converts a RuntimFeature to a Feature<T>
     * 
     * @tparam T The type of the object to be wrapped
     * @return Feature<T> The converted Feature<T>
     */
    template <typename T, typename = std::enable_if_t<!std::is_same_v<T, Reflect::DynamicObject>>>
    operator Feature<T>() const
    {
        return Feature<T>(Reflect::cast<T>(this->param.value()));
    }

    /**
     * @brief retrieve a data member of the wrapped object and 
     * register the retrieval of the data member in the feature tree
     * 
     * This will return the data member wrapped in a Feature and register
     * the dependency of the returned feature to this.
     * 
     * @param memberName  The string representation of the member name
     * @return RuntimeFeature The data member wrapped in a Feature
     */
    decltype(auto) get(std::string const& memberName) const
    {
        return eval(
            [=](Reflect::DynamicObject const& wrapped){
                return wrapped.get(memberName);
            },
            *this
        );
    }

    /**
     * @brief invoke a member function of the wrapped object and register
     * the dependencies of the outputs on this in the feature tree
     * 
     * @tparam Args The argument types expected by the member function
     * @param memberFunName The string representation of the member function
     * @param args The arguments of the member function
     * @return RuntimeFeature The return value of the member function 
     */
    template <typename... Args>
    decltype(auto) invoke(std::string const& memberFunName, Feature<Args> const&... args) const
    {
        return eval(
            [=](Reflect::DynamicObject const& wrapped, auto const&... arguments){
                return wrapped.invoke(memberFunName, arguments...);
            },
            *this,
            args...
        );
    }

};

/**
 * @brief C++ 17 deduction guides for RuntimeFeature
 * 
 * @tparam Args The constructor arguments
 */
template <typename... Args>
Feature(std::string const&, Args&&...) -> Feature<Reflect::DynamicObject>;

/**
 * @brief typedef for RuntimeFeature
 * @ingroup dynamic
 */
using RuntimeFeature = Feature<Reflect::DynamicObject>;

}