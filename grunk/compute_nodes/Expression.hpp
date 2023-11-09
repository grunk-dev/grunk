#pragma once 

#include <grunk/DynamicFeature.hpp>
#include <grunk/common/parametric_core.hpp>

#include <yaml-cpp/yaml.h>
#include "muParser.h"

namespace grunk {

    class Recipe;
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
    class Expression : public parametric::ComputeNode<Expression>
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

        void connect_inputs(std::vector<DynamicFeature> const& inputs) {
            for (auto const& input : inputs) {
                depends_on(input.param());
            }
        };

        DynamicFeature initialize_results() const {
            return DynamicFeature(parametric::new_param<reflect::DynamicObject>(), reflect::resolve<double>());
        }

        void connect_results(DynamicFeature const& res) {
            computes(res.param());
        }

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
         * @return The ::grunk::DynamicFeature returned by the ::grunk::Expression.
         */
        static DynamicFeature deserialize(
            YAML::Node const&,
            Recipe const&
        );

    private:
        std::string const expr;
    };

}  // namespace grunk
