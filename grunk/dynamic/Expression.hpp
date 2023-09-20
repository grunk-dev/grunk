#pragma once 

#include <grunk/dynamic/DynamicFeature.hpp>
#include <grunk/dynamic/FeatureContainer.hpp>

#include <grunk/parametric_core.hpp>
#include <yaml-cpp/yaml.h>
#include "muParser.h"

namespace grunk {

    class Expression;

    /**
     * @brief The Expression class represents an expression to be evaluated at runtime
     *
     * Similarly to the DynamicAction class, it represents a special type of a compute node
     * in a parametric tree.
     *
     * The implementation is based on muparsers and all default functions, constants and operators
     * of muparser are supported in the expressions.
     *
     * The class has a private constructor, as it should alsways be created using the factory
     * function ::grunk::expression.
     *
     * @ingroup dynamic_advanced
     */
    class Expression : public parametric::ComputeNode<
                                  Expression,
                                  parametric::Results<reflect::DynamicObject>, /* Results are ignored*/
                                  parametric::Arguments<std::vector<reflect::DynamicObject>> /* Arguments are ignored by derived class */
                              >
    {
        /**
         * @brief Construct a new Expression instance
         * @param id The id used for the output of the exression
         * @param expression A string representing the expression. The unknowns must correspond to the ids of the input arguments.
         * @param in The input DynamicFeatures
         */
        Expression(
            std::string const& id,
            std::string const& expression
        );

    public:

        // factory method
        friend DynamicFeature expression(
            std::string const& id, 
            std::string const& expr, 
            std::vector<DynamicFeature> const& args
        );

        /**
         * @brief This function evaluates the expression and caches the output
         */
        void eval() const override final;

        /**
         * @brief serialize an expression to yaml. It will be serialized with the
         * reserved tag "expr". The yaml node consists of a list with two elements. The
         * first is the id of the output DynamicFeature. The second is the string representing
         * the expression.
         * @return A yaml-string representing the expression to be used in a grunk recipe
         */
        std::string serialize() const override final;

        void post_connect() const;

        /**
         * @brief deserializes a yaml-representation, e.g. from a grunk recipe to an instance
         * of ::grunk::ExpressionPtr, which in turn wraps the corresponding ::grunk::Expression.
         * @return The ::grunk::ExpressionPtr wrapping the corresponding ::grunk::Expression.
         */
        static DynamicFeature deserialize(
            YAML::Node const&,
            FeatureContainer const&
        );

    private:
        std::string const expr;
    };

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

}  // namespace grunk
