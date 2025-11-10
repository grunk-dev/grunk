#pragma once 

#include "grunk/core/parametric_core.hpp"
#include "grunk/core/ResultHolder.hpp"
#include "grunk/recipe/Recipe.hpp"

namespace grunk {

class RecipeAction : public parametric::ComputeNode<RecipeAction>
{
    friend class RecipeCaller;

private:

    decltype(auto) result() const;
    decltype(auto) input_recipe() const;
    decltype(auto) input_feature(int i) const;

public:

    RecipeAction(std::string const& name);

    void connect_inputs(Feature<Recipe> const& recipe, std::unordered_map<std::string, DynamicFeature> const& input_map);

    Feature<Recipe> initialize_results() const;

    void connect_results(Feature<Recipe> const& res);

    void eval() const override;

    std::string serialize() const override final;

private:
    std::string name;
    std::vector<std::string> target_features;
};

} // namespace 