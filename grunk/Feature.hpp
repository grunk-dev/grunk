/**
 * @file Feature.hpp
 *
 * Declaration and Definition of the Feature class.
 *
 * @defgroup static
 * @defgroup advanced
 */

#pragma once

#include <grunk/FeatureBase.hpp>

#include <reflect/reflect.hpp>
#include <type_traits>

namespace grunk {

//forward declarations
template <typename T>
class Feature;

template <typename F, typename... Args>
class Action;

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

    /**
     * @brief evaluates to true if a type is reflect::DynamicFunction
     */
    template<typename F>
    constexpr bool is_dynamic_callable_v = 
        std::is_base_of_v<reflect::DynamicFunction, F> ||
        std::is_base_of_v<reflect::OverloadSet, F>;
}

// forward declaration
template <typename F,
          typename = std::enable_if_t<
            !std::is_convertible_v<std::decay_t<F>, std::string>
            && !details::is_dynamic_callable_v<std::decay_t<F>>
          >,
          typename... Args>
decltype(auto) action(std::string const& id, F const& fun, Args&&... args);

/**
 * @brief The Feature class template represents a feature node in the 
 * feature tree. 
 *
 * In addition to the dependency management provided by the parametric library,
 * this class provides an interface to retrieve data members and invoke member
 * functions on the wrapped object and registering this action in the feature 
 * tree.
 * 
 * @tparam T The type of the wrapped object
 * 
 * @ingroup static
 */
template <typename T>
class Feature : public FeatureBase<T>
{
public:
    /**
     * @brief The type of the wrapped value
     */
    using value_type = T;

    /**
     * @brief Construct a new Feature object from an instance of type T.
     * 
     * @param id The id of the Feature
     * @param t The object to be wrapped
     */
    Feature(std::string const& id, T&& t)
     : FeatureBase<T>(id, std::forward<T>(t))
    {}

    /**
     * @brief Construct a new Feature object from a parametic::param<T>
     * 
     * @param p The parametric::param<T> to be wrapped
     */
    Feature(parametric::param<T>&& p)
     : FeatureBase<T>(std::forward<parametric::param<T>>(p))
    {}

    /**
     * @brief Construct a new Feature<T> object given constructor arguments
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
     * Feature<Foo> z(x, y);
     * \endcode
     *
     * Whenever one of the constructor arguments x or y changes, the Feature z will 
     * be marked for lazy reconstruction.
     * 
     * @tparam Args Constructor argument types for T
     * @param id The id of the Feature
     * @param args input Features for the constructor of T
     */
    template <
        typename... Args,
        typename = std::enable_if_t<!(std::is_same_v<Args, reflect::DynamicObject> || ...)>
    >
    Feature(std::string const& id, Feature<Args> const&... args)
     : Feature(
        action(
            id,
            [](Args const&... in){

                static_assert(std::is_constructible_v<T, Args...>, "T must be constructable from Args\n.");
                
                return T(in...);
            }, 
            args...
        ).output()
       )
    {};

    /**
     * @brief retrieve a data member of the wrapped object and 
     * register the retrieval of the data member in the feature tree
     * 
     * This will return the data member wrapped in a Feature and register
     * the dependency of the returned feature to this.
     *
     * @tparam MemberPtr Type of the MemberPointer
     * @param id id to be assigned to the Feature wrapping the retrieved data member
     * @param ptr Pointer to the data member of the the wrapped object's type
     * @return decltype(auto) The data member wrapped in a Feature
     */
    template <typename MemberPtr>
    decltype(auto) get(std::string const& id, MemberPtr ptr) const
    {
        return action(
            id,
            [=](auto const& wrapped){ 
                return wrapped.*ptr; 
            }, 
            *this
        );
    }

    /**
     * @brief invoke a member function of the wrapped object and register
     * the dependencies of the outputs on this in the feature tree
     * 
     * @tparam MemberFunPtr Type of the member funtion pointer
     * @tparam Args The arguments expected by the member function
     * @param id id to be assigned to the Feature wrapping the return value(s)
     * @param funPtr The pointer to the member function of the wrapped object's type
     * @param args The arguments expected by the member function, wrapped in Feature instances
     * @return decltype(auto) The return value of the member function, wrapped in a Feature instance
     */
    template <typename MemberFunPtr, typename... Args>
    decltype(auto) invoke(std::string const& id, MemberFunPtr funPtr, Feature<Args> const&... args) const
    {
        return action(
            id,
            [=](T const& wrapped, auto const&... arguments){
                return (wrapped.*funPtr)(arguments...);
            },
            *this,
            args...
        );
    }
};

/**
 * @brief C++17 deduction guide for Feature<T>
 * 
 * @tparam T The type of the wrapped object
 */
template <typename T>
Feature(std::string const&, T&&) -> Feature<T>;

} //namespace grunk

#include "compute_nodes/Action.hpp"
