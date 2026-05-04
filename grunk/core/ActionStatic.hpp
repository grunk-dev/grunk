// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/core/Feature.hpp"
#include "ResultHolder.hpp"

namespace grunk {

namespace details {

struct ActionFactory;

} // namespace details

/**
 * @ingroup advanced_core
 * @brief The Action class template is a compute node in the feature tree that wraps a referentially transparent function and its arguments. It evaluates the function for the given arguments and caches the output as features. The function can return either a single value, a tuple of values or void. The Action class template inherits from parametric::ComputeNode and implements the eval method to perform the evaluation and caching of the outputs.
 * 
 * @tparam F The type of the wrapped function, e.g. a lambda, a function pointer or a std::function
 * @tparam Args The types of the arguments expected by the wrapped function, e.g. double, std::string or even user defined types
 */
template <typename F, typename... Args>
class Action : public ::parametric::ComputeNode<
                          Action<F, Args...>,
                          parametric::Results<std::invoke_result_t<F, Args const&...>>,
                          parametric::Arguments<Args...>
                      >
{
public:

    static_assert(std::is_invocable_v<F, Args const& ...>, "\n\nFunction is not invocable with const references. "
        "Actions can only be used with referentially transparent functions.\n\n");

    /**
     * @brief The return type of the wrapped function
     */
    using ReturnType = std::invoke_result_t<F, Args const&...>;

    friend struct details::ActionFactory;

    /**
     * @brief The number of outputs of the wrapped function. This is 0 for void functions, 1 for functions that return a single value and n for functions that return a tuple of n values.
     */
    static constexpr size_t nresults = std::tuple_size_v<parametric::Results<ReturnType>>;

    /**
     * @brief evaluates the function and caches the output.
     * 
     */
    void eval() const override final
    {

        if constexpr (nresults > 1) {
            auto ret = call(std::make_index_sequence<sizeof...(Args)>{});
            this->set_outputs(std::make_index_sequence<nresults>{}, ret);
        }
        else if constexpr (nresults == 1) {
            this->set_output<0>(call(std::make_index_sequence<sizeof...(Args)>{}));
        }
        else {
            call(std::make_index_sequence<sizeof...(Args)>{});
        }
    }

private:

    /**
     * @brief Construct a new Action object
     * 
     * @param f  the function to be wrapped
     * @param args The arguments of the function wrapped in Feature instances
     */
    Action(F const& f)
     : function(f)
    {}

    /**
     * @brief internal helper function to query the ith index of the input
     * arguments using the index trick
     * 
     * @tparam I indices of the input arguments
     * @return ReturnType the retun value of the wrapped function
     */
    template <size_t... I>
    ReturnType call(std::index_sequence<I...>) const
    {
        return std::invoke(function, this->template arg<I>().value()...);
    }

    /**
     * @brief internal helper function to set the outputs with the new calcuation
     * result using the index trick. Here it is assumed that the function returns 
     * a tuple.
     * 
     * @tparam I indices of the output values
     * @param ret The tuple returned by the wrapped function
     */
    template <typename T, size_t... I>
    void set_outputs(std::index_sequence<I...>, T const& ret) const
    {
        (set_output<I>(std::get<I>(ret)), ...);
    }

    /**
     * @brief internal helper function to set the output of an individual output
     * using the index trick
     * 
     * @tparam I index of the output
     * @tparam T type of the output
     * @param t new value for the output
     */
    template <size_t I, typename T>
    void set_output(T const& t) const
    {
        if (auto r =  this->template res<I>(); r) {
            r->set_value(t);
        }
    }

    F const function;
};

namespace details {


/**
 * @brief The ActionFactory struct is an internal factory for creating Action
 * instances.
 *
 * It is a proxy class used in the free factory functions action. Factory functions are
 * needed, because Actions should always be wrapped in a parametric::compute_node_ptr
 * and the private constructor of ALgorithm makes sure that there is no misuse. The
 * factory function parametric::new_node does not work with the templated constructors of the
 * Action class, so we need new factory functions.
 *
 * The proxy factory is needed, because the factory functions action must be templated, and
 * templated friend functions are a pain in the ass. This way we have a non-templated friend
 * struct with templated member functions.
 *
 * @ingroup advanced_core
 */
struct ActionFactory
{


    /**
     * @brief construct a new Action instance.
     * 
     * @tparam F The type of the wrapped function
     * @tparam Args The types of the arguments expected by the wrapped function
     * @param fun The function to be wrapped in an Action instance
     * @param args The arguments wrapped in Features to be passed to the function on evaluation
     * @return ResultHolder wrapping the outputs of the Action
     */
    template <typename F,
              typename... Args>
    static ResultHolder<Action<F, Args...>> new_action(F const& fun, Feature<Args> const&... args)
    {
        using MyAction = Action<F, Args...>;
        return ResultHolder<MyAction>(
            parametric::compute(
                std::shared_ptr<MyAction>(new MyAction(fun)),
                args...
            )
        );
    }

};


/**
 * @brief Given a function and some features in the feature tree, this
 * function creates an Action instance representing the evaluation
 * of the input function for the input features.
 *
 * This function accepts features as arguments for the functions, as well
 * as instances that are not wrapped in features. Internally, the latter will
 * be wrapped in an unnamed/anonymous feature
 *
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function,
             In particular, the function must be invokable on const
             references.
 * @tparam Args The types of the arguments expected by the input function
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ResultHolder wrapping the outputs of the Action
 *
 * @ingroup advanced_core
 */
template <typename F,
          typename... Args>
ResultHolder<Action<F, Args...>> action(F const& fun, Feature<Args> const&... args)
{
    return ActionFactory::new_action(fun, args...);
}

} // namespace details

/**
 * @brief Given a function and some features in the feature tree, this
 * function creates an Action instance representing the evaluation
 * of the input function for the input features.
 *
 * This function accepts features as arguments for the functions, as well
 * as instances that are not wrapped in features. Internally, the latter will
 * be wrapped in an unnamed/anonymous feature
 *
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function,
             In particular, the function must be invokable on const
             references.
 * @tparam Args The types of the arguments expected by the input function
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ResultHolder wrapping the outputs of the Action
 *
 * @ingroup core
 */
template <typename F,
          typename... Args>
inline decltype(auto) action(F const& fun, Args&&... args)
{
    return details::ActionFactory::new_action(fun, details::to_feature(std::forward<Args>(args))...);
}

} // namespace grunk
