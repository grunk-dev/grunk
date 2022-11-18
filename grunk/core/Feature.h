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

#include <reflect/reflect.hpp>
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
    constexpr bool is_dynamic_function_v = std::is_base_of_v<reflect::DynamicFunction, F>;
}

// forward declaration
template <typename F,
          typename = std::enable_if_t<
            !std::is_convertible_v<std::decay_t<F>, std::string>
            && !details::is_dynamic_function_v<std::decay_t<F>>
          >,
          typename... Args>
decltype(auto) eval(std::string const& id, F const& fun, Args&&... args);

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
    FeatureBase(std::string const& id, T&& t)
     : m_param(parametric::new_param(std::forward<T>(t)), id)
    {}

    /**
     * @brief Construct a new FeatureBase object from a parametric::param<T>
     * 
     * @param p a parametric::param<T>
     */
    FeatureBase(parametric::param<T>&& p)
     : m_param(std::forward<parametric::param<T>>(p))
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
        return m_param.is_valid();
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
        return m_param.value();
    }

    /**
     * @brief returns the id of the feature
     * 
     * @return std::string 
     */
    std::string id() const {
        return m_param.id();
    }

    void set_id(std::string const& s) {
        m_param.set_id(s);
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
        return m_param.change_value();
    }

    /**
     * @brief returns a 
     * 
     * @return parametric::param<T> const& 
     */
    parametric::param<T> const& param() const {
        return m_param;
    }

protected:

    parametric::param<T> m_param;
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
     * @param args input Features for the constructor of T
     */
    template <
        typename... Args,
        typename = std::enable_if_t<!(std::is_same_v<Args, reflect::DynamicObject> || ...)>
    >
    Feature(std::string const& id, Feature<Args> const&... args)
     : Feature(
        eval(
            id,
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
            "", //To Do
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
            "", // TO DO
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

#include "Algorithm.h"
