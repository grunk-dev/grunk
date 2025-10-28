#pragma once 

#include "grunk/dynamic/internal/parametric_core.hpp"
#include "grunk/dynamic/internal/ResultHolder.hpp"
#include "grunk/recipe/Recipe.hpp"

namespace grunk {

class RecipeAction : public parametric::ComputeNode<RecipeAction>
{
    friend class RecipeCaller;

private:

    inline decltype(auto) result() const {
        return this->template res<object>(0);
    }

    inline decltype(auto) argument(int i) const {
        return this->template arg<object>(i);
    }

public:

    using ReturnType = grunk::object;

    RecipeAction(std::string const& name) : name(name) {}

    void connect_inputs(DynamicFeature const& recipe, std::unordered_map<std::string, DynamicFeature> const& input_map)
    {
        depends_on(recipe);
        for (auto const& [key, value] : input_map) {
            target_features.push_back(key);
            depends_on(value);
        }
    }

    DynamicFeature initialize_results() const
    {
        return feature<object>({});
    }

    void connect_results(DynamicFeature const& res)
    {
        computes(res);
    }

    void eval() const override
    {

        auto recipe_obj = argument(0).value();
        Recipe const& recipe_from = recipe_obj.as<Recipe const&>();

        //TODO: It this expensive?
        Recipe cloned = recipe_from.clone();

        for (size_t i = 1; i < this->num_parents(); ++i) {
            auto const& arg = argument(i).value();
            auto target = cloned.get_feature(target_features[i]);
            target.set_value(arg);
        }

        // transform to output
        if (auto output = result(); output) {
            sol::state_view lua(recipe_obj.lua_state());
            auto cloned_obj = sol::make_object(lua, cloned);
            output->set_value(cloned_obj);
        }
    }

    std::string serialize() const override final
    {
        //TODO
        return name;
    }

private:
    std::string name;
    std::vector<std::string> target_features;
};

} // namespace 