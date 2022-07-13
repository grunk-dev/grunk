/**
 * @file Feature.h
 */

#pragma once

#include <parametric/core.hpp>

#include "RuntimeFunction.h"

namespace grunk {

//forward declarations
template <typename T>
class Feature;

template <typename F, typename... Args>
class Algorithm;

template<typename F, typename... Args>
using AlgorithmPtr = parametric::compute_node_ptr<Algorithm<F, Args...>>;

namespace details {
    template<typename> constexpr bool is_runtime_function_v = false;

    template<typename F>
    constexpr bool is_runtime_function_v<RuntimeFunction<F>> = true;
}

template <typename F,
          typename = std::enable_if_t<
            !std::is_convertible_v<std::decay_t<F>, std::string>
            && !details::is_runtime_function_v<std::decay_t<F>>
          >,
          typename... Args>
AlgorithmPtr<F, Args...> eval(F const& fun, Feature<Args> const&... args);

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

template <typename T>
Feature(T&&) -> Feature<T>;

} //namespace grunk

#include "Algorithm.h"
