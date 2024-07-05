#pragma once 

#include <grunk/compute_nodes/Expression.hpp>

namespace grunk {

    /**
     * @brief expression creates an instance of ::grunk::Expression and returns the outpt in a ::grunk::DynamicFeature.
     *
     * This function creates a compute node in a parametric tree representing the evaluation of an expression.
     * The expression may contain variable names corresponding to the ids of ::grunk::DynamicFeature instances, which are
     * passed as inputs to the expression. The expression may use any functions, operators or constants supported by
     * the muparser library.
     *
     * @param id The id of the output ::grunk::DynamicFeature
     * @param expr A string representing the expression.
     * @param args A vector of ::grunk::DynamicFeature instances, which are the inputs to the expression.
     * @return ::grunk::DynamicFeature, the evaluated expression as a node in the feature tree.
     * @ingroup dynamic
     */
    DynamicFeature expression(std::string const& id, std::string const& expr, std::vector<DynamicFeature> const& args);

    /**
     * @brief expression creates an instance of ::grunk::Expression and returns the outpt in a ::grunk::DynamicFeature.
     *
     * This function creates a compute node in a parametric tree representing the evaluation of an expression.
     * The expression may contain variable names corresponding to the ids of ::grunk::DynamicFeature instances, which are
     * passed as inputs to the expression. The expression may use any functions, operators or constants supported by
     * the muparser library.
     *
     * @param id The id of the output ::grunk::DynamicFeature
     * @param expr A string representing the expression.
     * @param args A vector of ::grunk::DynamicFeature instances, which are the inputs to the expression.
     * @return ::grunk::DynamicFeature, the evaluated expression as a node in the feature tree.
     * @ingroup dynamic_advanced
     */
    template <
    typename... Args,
    typename = std::enable_if_t<!(sizeof...(Args) == 1 && (std::is_same_v<std::vector<DynamicFeature>, std::decay_t<Args>> && ...))>
    >
    DynamicFeature expression(std::string const& id, std::string const& expr, Args&&... args)
    {
        auto to_feature = [](auto&& arg){
            using Arg = std::decay_t<decltype(arg)>;
            if constexpr (details::is_feature_v<Arg>){
                return std::forward<decltype(arg)>(arg);
            } else {
                return Feature("", reflect::DynamicObject(std::forward<Arg>(arg))); //TODO: Until we properly support unnamed features, this will be an empty string
            }
        };
        return expression(id, expr, std::vector<DynamicFeature>{to_feature(std::forward<Args>(args))...});
    }

} // namespace grunk