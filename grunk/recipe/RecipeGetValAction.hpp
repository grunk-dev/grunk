// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/core/parametric_core.hpp"
#include "grunk/core/ResultHolder.hpp"
#include "grunk/recipe/Recipe.hpp"

namespace grunk {

class RecipeGetValAction : public parametric::ComputeNode<RecipeGetValAction>
{
    friend class RecipeCaller;

private:

    decltype(auto) result() const;

    decltype(auto) argument() const;

public:

    RecipeGetValAction(std::string const& name);

    void connect_inputs(Feature<Recipe> const& recipe, std::string const& key);

    DynamicFeature initialize_results() const;

    void connect_results(DynamicFeature const& res);

    void eval() const override;

    std::string serialize() const override final;

private:
    std::string name;
    std::string source_feature;
};

} // namespace 