// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

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
            output->set_value(recipe.get_feature(source_feature).value());
        }
    }

    std::string RecipeGetValAction::serialize() const
    {
        std::string recipe_caller_name = argument().id();
        if (auto output = result(); output) {
            std::string ret = recipe_caller_name + "." + source_feature;
            if (!output->id().empty()) {
                return output->id() + " = " + ret;
            } else {
                return ret;
            }
        }
        return "";
    }

} // namespace grunk