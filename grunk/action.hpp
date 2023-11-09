#pragma once 

#include <grunk/compute_nodes/Action.hpp>
#include <grunk/compute_nodes/DynamicAction.hpp>

namespace grunk {

namespace details {

/**
 * @brief Given a function and some features in the feature tree, this 
 * function creates an Action instance representing the evaluation
 * of the input function for the input features.
 *
 * This function accepts only features as arguments to the given function.
 * 
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function, 
             In particular, the function must be invokable on const 
             references.
 * @tparam Args The types of the arguments expected by the input function
 * @param id The id used for the output of the function
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ResultHolder wrapping the outputs of the Action
 */
template <typename F,
          typename = std::enable_if_t<
            !std::is_convertible_v<std::decay_t<F>, std::string>
            && !details::is_dynamic_callable_v<std::decay_t<F>>
          >,
          typename... Args>
decltype(auto) action(std::string const& id, F const& fun, Feature<Args> const&... args)
{
    return details::ActionFactory::new_action(id, fun, args...);
}

} //namespace details

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
 * @param id the id of the output Feature
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ResultHolder wrapping the outputs of the Action
 *
 * @ingroup static
 */
template <typename F,
          typename,
          typename... Args>
decltype(auto) action(std::string const& id, F const& fun, Args&&... args)
{
    auto to_feature = [](auto&& arg){
        using Arg = std::decay_t<decltype(arg)>;
        if constexpr (details::is_feature_v<Arg>){
            return arg;
        } else {
            return Feature("", std::forward<Arg>(arg)); //TODO: Until we properly support unnamed features, this will be an empty string
        }
    };
    return details::action(id, fun, to_feature(args)...);
}

/**
 * @brief Given a function and a vector of features in the feature 
 * tree, this function creates a DynamicAlgoritm instance representing the 
 * evaluation of the input function for the input features.
 * 
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function, 
             In particular, the function must be invokable on const 
             references.
 * @param id The id of the output ::grunk::DynamicFeature
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ResultHolder A special wrapper class around the output features.
 * @ingroup dynamic_advanced
 */
ResultHolder<DynamicAction> action(std::string const& id, reflect::DynamicFunction const& fun, std::vector<DynamicFeature> const& args);

namespace details {

template<typename T>
struct is_string
        : public std::disjunction<
                    std::is_same<char *, std::decay_t<T>>,
                    std::is_same<const char *, std::decay_t<T>>,
                    std::is_same<std::string, std::decay_t<T>>,
                    std::is_same<std::string_view, std::decay_t<T>>
        > 
{};

template <typename T>
constexpr bool is_string_v = is_string<T>::value;

template <typename Arg>
DynamicFeature to_dynamic_feature(Arg&& arg)
{
    using T = std::decay_t<Arg>;
    if constexpr (details::is_feature_v<T>){
        return std::forward<Arg>(arg);
    } else {
        // special handling of string-like types: We want to always conert them to String first
        if constexpr (details::is_string_v<Arg>) {
            return Feature("", reflect::DynamicObject(helper::String(std::forward<Arg>(arg))));
        } else {
            return Feature("", reflect::DynamicObject(std::forward<Arg>(arg)));
        }
    }
}

} // namespace details

/**
 * @brief Given a function and some features in the feature tree, this 
 * function creates a DynamicAlgoritm instance representing the evaluation
 * of the input function for the input features.
 *
 * This function accepts features as arguments for the functions, as well
 * as instances that are not wrapped in features. Internally, the latter will
 * be wrapped in an unnamed/anonymous feature
 * 
 * @tparam Args The types of the arguments expected by the input function
 * @param id The id of the output ::grunk::DynamicFeature
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ResultHolder<DynamicAction> The returned ResultHolder wrapping the outputs
 * @ingroup dynamic_advanced
 */
template <
    typename... Args,
    typename = std::enable_if_t<!(sizeof...(Args) == 1 && (std::is_same_v<std::vector<DynamicFeature>, std::decay_t<Args>> && ...))>
>
ResultHolder<DynamicAction> action(std::string const& id, reflect::DynamicFunction const& fun, Args&&... args)
{
    return action(
        id, 
        fun, 
        std::vector<DynamicFeature>{details::to_dynamic_feature(std::forward<Args>(args))...}
    );
}

namespace details {

template <typename T>
reflect::DynamicFunction::SpecifiedArgument to_specified_argument(T&& f)
{
    using F = std::decay_t<T>;
    reflect::DynamicFunction::ArgumentSpecifier spec 
        = reflect::DynamicFunction::ArgumentSpecifier::PtrOrRefToConst;
    if constexpr (std::is_rvalue_reference_v<T>) {
        spec = reflect::DynamicFunction::ArgumentSpecifier::Value;
    }

    if constexpr ( std::is_same_v<F, DynamicFeature>) {
        if (f.get_type_descriptor() == nullptr) {
            throw std::logic_error("Unexpected error: Unknown type of dynamic feature.");
        }
        return reflect::DynamicFunction::SpecifiedArgument{
            f.get_type_descriptor(),
            spec
        };
    } else if constexpr (details::is_feature_v<F>) {
        return reflect::DynamicFunction::SpecifiedArgument{
            reflect::resolve<typename F::value_type>(),
            spec
        };
    } else {
        return reflect::DynamicFunction::SpecifiedArgument{
            reflect::resolve<F>(),
            spec
        };
    }
}

} // namespace details

/**
 * @brief Given a string identifier of a function, that has previously been registered
 * in the static function registry as well as input features of the feature tree, 
 * this function represents the evaluation of the registered function when it gets 
 * passed the input features.
 * 
 * @tparam Args The types of the arguments expected by the registered function
 * @param id The id of the output ::grunk::DynamicFeature
 * @param name The string identifier of the registered function
 * @param args The input Features
 * @return ResultHolder<DynamicAction> The returned ResultHolder wrapping the outputs
 *
 * @ingroup dynamic
 */
template <typename... Args>
ResultHolder<DynamicAction> action(std::string const& id, std::string const& name, Args&&... args)
{
    std::vector<reflect::DynamicFunction::SpecifiedArgument> specified_args{
        details::to_specified_argument(std::forward<Args>(args))...
    };

    auto const& overload = reflect::resolve_function(name);
    auto const& function = overload.resolve(specified_args);
    return action(id, function, std::forward<Args>(args)...);
}

ResultHolder<DynamicAction> action(std::string const& id, std::string const& name, std::vector<DynamicFeature> const& args);


} // namespace grunk
