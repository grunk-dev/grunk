#pragma once 

#include "grunk/dynamic/internal/parametric_core.hpp"
#include "grunk/dynamic/internal/ResultHolder.hpp"
#include "grunk/recipe/Recipe.hpp"

namespace grunk {

class RecipeAction : public parametric::ComputeNode<RecipeAction>
{
    friend class RecipeCaller;

private:

    decltype(auto) result() const;

    decltype(auto) argument(int i) const;

public:

    RecipeAction(std::string const& name);

    void connect_inputs(DynamicFeature const& recipe, std::unordered_map<std::string, DynamicFeature> const& input_map);

    DynamicFeature initialize_results() const;

    void connect_results(DynamicFeature const& res);

    void eval() const override;

    std::string serialize() const override final;

private:
    std::string name;
    std::vector<std::string> target_features;
};

} // namespace 