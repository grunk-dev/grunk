#pragma once 

#include <grunk/dynamic/DynamicFeature.hpp>
#include <grunk/dynamic/FeatureContainer.hpp>

#include <grunk/parametric_core.hpp>
#include <yaml-cpp/yaml.h>
#include "muparserx/mpParser.h"

namespace grunk {

    class Expression;

    using ExpressionPtr = parametric::compute_node_ptr<Expression>;

    class Expression : public parametric::ComputeNode
    {
        Expression(
            std::string const& id,
            std::string const& expression,
            std::vector<DynamicFeature> const& in
        );

    public:

        // factory method
        friend ExpressionPtr expression(
            std::string const& id, 
            std::string const& expr, 
            std::vector<DynamicFeature> const& args
        );

        void eval() const override final;

        inline DynamicFeature output(size_t idx = 0) const
        {
            return DynamicFeature(out, reflect::resolve<double>());
        }

        std::string serialize() const override final;

        static ExpressionPtr deserialize(
            YAML::Node const&,
            FeatureContainer const&
        );

    private:
        std::vector<DynamicFeature> const inputs;
        parametric::OutputParam<reflect::DynamicObject> mutable out;
        std::string const expr;
    };

    ExpressionPtr expression(std::string const& id, std::string const& expr, std::vector<DynamicFeature> const& args);

    template <
    typename... Args,
    typename = std::enable_if_t<!(sizeof...(Args) == 1 && (std::is_same_v<std::vector<DynamicFeature>, std::decay_t<Args>> && ...))>
    >
    ExpressionPtr expression(std::string const& id, std::string const& expr, Args&&... args)
    {
        auto to_feature = [](auto&& arg){
            using Arg = std::decay_t<decltype(arg)>;
            if constexpr (details::is_feature_v<Arg>){
                return arg;
            } else {
                return Feature("", reflect::DynamicObject(std::forward<Arg>(arg))); //TODO: Until we properly support unnamed features, this will be an empty string
            }
        };
        return expression(id, expr, std::vector<DynamicFeature>{to_feature(std::forward<Args>(args))...});
    }

}  // namespace grunk