#pragma once 

#include <grunk/core/Action.hpp>
#include <grunk/dynamic/DynamicFeature.hpp>

#include "muparserx/mpParser.h"

namespace grunk {

    class Expression : public parametric::ComputeNode
    {
    public:
        Expression(
            std::string const& id,
            std::string const& expression,
            std::vector<DynamicFeature> const& in
        );

        void eval() const override final;

        std::string serialize() const override final;

    private:
        std::vector<DynamicFeature> const inputs;
        parametric::OutputParam<reflect::DynamicObject> mutable output;
        std::string const expr;
    };

}  // namespace grunk