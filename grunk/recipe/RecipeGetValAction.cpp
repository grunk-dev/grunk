#include "RecipeGetValAction.hpp"

namespace grunk {

    decltype(auto) RecipeGetValAction::result() const 
    {
        return this->template res<object>(0);
    }

    decltype(auto) RecipeGetValAction::argument() const
    {
        return this->template arg<Recipe>(0);
    }

    RecipeGetValAction::RecipeGetValAction(std::string const& name)
     : name(name) 
    {}

    void RecipeGetValAction::connect_inputs(Feature<Recipe> const& recipe, std::string const& key)
    {
        depends_on(recipe);
        source_feature = key;
    }

    DynamicFeature RecipeGetValAction::initialize_results() const
    {
        return feature<object>({});
    }

    void RecipeGetValAction::connect_results(DynamicFeature const& res)
    {
        computes(res);
    }

    void RecipeGetValAction::eval() const
    {

        Recipe const& recipe = argument().value();

        // transform to output
        if (auto output = result(); output) {
            output->set_value(recipe[source_feature]);
        }
    }

    std::string RecipeGetValAction::serialize() const
    {
        //TODO
        return name;
    }

} // namespace grunk