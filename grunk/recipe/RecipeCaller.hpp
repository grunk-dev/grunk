#pragma once 

#include "grunk/recipe/Recipe.hpp"

namespace grunk {

class RecipeCaller
{
public:

    RecipeCaller(std::string const& name, Feature<Recipe> const& recipe);

    struct Proxy {
        std::string key;
        RecipeCaller& rc;

        void operator=(DynamicFeature const& other);

        operator DynamicFeature();
    };

    bool locked() const;

    void create_recipe_action();


private:
    std::string name;
    Feature<Recipe> source_recipe;
    std::unordered_map<std::string, DynamicFeature> inputs;
    std::optional<Feature<Recipe>> target_recipe;
};

} // namespace grunk