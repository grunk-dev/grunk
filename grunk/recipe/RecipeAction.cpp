#include "RecipeAction.hpp"

namespace grunk {

    decltype(auto) RecipeAction::result() const 
    {
        return this->template res<Recipe>(0);
    }

    decltype(auto) RecipeAction::input_recipe() const
    {
        return this->template arg<Recipe>(0);
    }

    decltype(auto) RecipeAction::input_feature(int i) const
    {
        return this->template arg<object>(i);
    }

    RecipeAction::RecipeAction(std::string const& name)
     : name(name) 
    {}

    void RecipeAction::connect_inputs(Feature<Recipe> const& recipe, std::unordered_map<std::string, DynamicFeature> const& input_map)
    {
        depends_on(recipe);
        for (auto const& [key, value] : input_map) {
            target_features.push_back(key);
            depends_on(value);
        }
    }

    Feature<Recipe> RecipeAction::initialize_results() const
    {
        return grunk::Feature(parametric::new_param<Recipe>());
    }

    void RecipeAction::connect_results(Feature<Recipe> const& res)
    {
        computes(res);
    }

    void RecipeAction::eval() const
    {
        Recipe const& recipe_from = input_recipe().value();

        //TODO: It this expensive?
        Recipe cloned = recipe_from.clone();

        for (size_t i = 1; i < this->num_parents(); ++i) {
            auto const& arg = input_feature(i).value();
            auto target = cloned.get_feature(target_features[i-1]);
            target.set_value(arg);
        }

        // transform to output
        if (auto output = result(); output) {
            output->set_value(cloned);
        }
    }

    std::string RecipeAction::serialize() const
    {
        //TODO
        return name;
    }

} // namespace grunk
