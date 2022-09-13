/**
 * @file Feature.h
 *
 * Declaration and Definition of the Feature class.
 *
 * @defgroup static
 * @defgroup advanced
 */

#pragma once

#include <parametric/core.hpp>

#include <reflect/Reflect.hpp>
#include <type_traits>

namespace grunk {

//forward declarations
template <typename T>
class Feature;

template <typename F, typename... Args>
class Algorithm;

/**
 * @brief AlgorithmPtr is a parametric::compute_node_ptr wrapping an Algorithm
 * instance
 * 
 * @tparam F The type of the function wrapped by the wrapped Algorithm instance
 * @tparam Args The arguments expected by the wrapped function.
 */
template<typename F, typename... Args>
using AlgorithmPtr = parametric::compute_node_ptr<Algorithm<F, Args...>>;

namespace details {

    /**
     * @brief evaluates to false if a type is not a Reflect::Function
     * 
     * @tparam typename the type to be checked.
     */
    template<typename> constexpr bool is_dynamic_function_v = false;

    /**
     * @brief evaluates to true if a type is Reflect::Function
     */
    template<>
    constexpr bool is_dynamic_function_v<Reflect::Function> = true;
}

// forward declaration
template <typename F,
          typename = std::enable_if_t<
            !std::is_convertible_v<std::decay_t<F>, std::string>
            && !details::is_dynamic_function_v<std::decay_t<F>>
          >,
          typename... Args>
decltype(auto) eval(F const& fun, Args&&... args);

/**
 * @brief A base class used by Feature<T> and the template specialization
 * Feature<RuntimeObject> aka RuntimeFeature.
 *
 * This class implements the common interface for all template realizations.
 * 
 * @tparam T The wrapped type of the feature
 *
 * @ingroup advanced
 */
template <typename T>
class FeatureBase {
public:

    template <typename F, typename... Args>
    friend class Algorithm;

    /**
     * @brief Construct a new FeatureBase object from an instance of type T
     * 
     * @param t The instance to be wrapped inside this feature
     */
    FeatureBase(T&& t)
     : param(parametric::new_param(std::forward<T>(t)))
    {}

    /**
     * @brief Construct a new FeatureBase object from a parametric::param<T>
     * 
     * @param p a parametric::param<T>
     */
    FeatureBase(parametric::param<T>&& p)
     : param(std::forward<parametric::param<T>>(p))
    {}

    /**
     * @brief returns true if the Feature's cache contains a valid value
     * and false, if it doesn't.
     *
     * A feature is invalid, if its ancestors are invalid, that is if the
     * computations upwards in the feature tree have not been performed, or
     * if an ancestor has been invalidated by a parameter change.
     * 
     * @return true if the cache contains a value
     * @return false if the cache does not contain a value
     */
    bool is_valid() const
    {
        return param.is_valid();
    }

    /**
     * @brief returns a const reference to the contained value.
     *
     * This triggers the computation of (part of) the feature tree, if 
     * the feature has been invalid before.
     * 
     * @return T const& reference to the wrapped object
     */
    T const& value() const
    {
        return param.value();
    }

    /**
     * @brief returns a non-const reference to the feature. 
     * 
     * This triggers invalidation of all descendents in the feature tree.
     *
     * When changing a non-independent feature maually using access_value, 
     * any changes in the ancestors get precedence: A following change of 
     * an ancestor of this feature will trigger the invalidation of this 
     * feature and the manual change will be overwritten.
     * 
     * @return T& reference to the wrapped object.
     */
    T& access_value()
    {
        return param.change_value();
    }

protected:

    parametric::param<T> param;
};

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
     * @brief Construct a new Feature object from an instance of type T.
     * 
     * @param t The object to be wrapped
     */
    Feature(T&& t)
     : FeatureBase<T>(std::forward<T>(t))
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
     * @param args input Features for the constructor of T
     */
    template <
        typename... Args,
        typename = std::enable_if_t<!(std::is_same_v<Args, Reflect::DynamicObject> || ...)>
    >
    Feature(Feature<Args> const&... args)
     : Feature(
        eval(
            [](Args const&... in){

                static_assert(std::is_constructible_v<T, Args...>, "T must be constructable from Args\n.");
                
                return T(in...);
            }, 
            args...
        )->get()
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
     * @param ptr Pointer to the data member of the the wrapped object's type
     * @return decltype(auto) The data member wrapped in a Feature
     */
    template <typename MemberPtr>
    decltype(auto) get(MemberPtr ptr) const
    {
        return eval(
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
     * @param funPtr The pointer to the member function of the wrapped object's type
     * @param args The arguments expected by the member function, wrapped in Feature instances
     * @return decltype(auto) The return value of the member function, wrapped in a Feature instance
     */
    template <typename MemberFunPtr, typename... Args>
    decltype(auto) invoke(MemberFunPtr funPtr, Feature<Args> const&... args) const
    {
        return eval(
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
Feature(T&&) -> Feature<T>;

} //namespace grunk

#include "Algorithm.h"
