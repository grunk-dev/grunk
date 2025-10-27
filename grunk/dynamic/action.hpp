#pragma once

#include "internal/ActionDynamic.hpp"
#include "internal/ActionStatic.hpp"
#include "internal/ResultHolder.hpp"
#include <utility>


namespace grunk {

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
 * @ingroup static
 */
template <typename F,
          typename... Args>
inline decltype(auto) action(F const& fun, Args&&... args)
{
    return details::ActionFactory::new_action(fun, details::to_feature(std::forward<Args>(args))...);
}

/**
 * @brief Given a string identifier of a function, that has previously been registered
 * in the static function registry as well as input features of the feature tree,
 * this function represents the evaluation of the registered function when it gets
 * passed the input features.
 *
 * @tparam Args The types of the arguments expected by the registered function
 * @param name The string identifier of the registered function
 * @param args The input Features
 * @return ResultHolder<DynamicAction> The returned ResultHolder wrapping the outputs
 *
 * @ingroup dynamic
 */
template <typename... Args>
inline ResultHolder<ActionDynamic> action(function_meta const& function, Args&&... args)
{
    using FirstArg = std::tuple_element<0, std::tuple<Args...>>;
    if constexpr (sizeof...(Args) == 1 && (std::is_same_v<std::decay_t<Args>, std::vector<DynamicFeature>> || ...)) {
        return details::DynamicActionFactory::new_action(function, args...);
    } else {
        return details::DynamicActionFactory::new_action(function, details::to_feature(std::forward<Args>(args))...);
    }
}

namespace details {

inline auto make_dynamic_action(sol::state const& lua, function_meta const& func)
{
    auto decorated_function = [func, &lua](sol::variadic_args va) -> grunk::DynamicFeature
    {
        
        auto raw_args = std::vector<sol::object>(va.begin(), va.end());

        // HACK: for unary operators, sol::variadic_args includes the table as the first argument
        // For serialization/deserialization consistency, we remove it here
        // Otherwise -x gets serializes as -xx
        if (func.get_name() == "grunk._dynamic_unm") {
            raw_args.erase(raw_args.begin());
        }

        std::vector<grunk::DynamicFeature> args;
        args.reserve(raw_args.size());

        // Use std::transform to convert variadic_args to std::vector<DynamicFeature>
        std::transform(
            raw_args.begin(), raw_args.end(),
            std::back_inserter(args),
            [](grunk::object const& obj) {
                if (obj.is<grunk::DynamicFeature>()) {
                    return obj.as<grunk::DynamicFeature>();
                } else {
                    return grunk::feature(obj);
                }
            }
            );

        return grunk::action(func, args).output();
    };
    return decorated_function;
}

} // namespace details

} // namespace grunk

#include "math.hpp"
