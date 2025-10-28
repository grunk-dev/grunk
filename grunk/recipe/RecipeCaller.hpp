#pragma once 

#include "grunk/recipe/Recipe.hpp"
#include "grunk/recipe/RecipeAction.hpp"

namespace grunk {

class RecipeCaller
{
public:

    RecipeCaller(std::string const& name, Recipe const& recipe);

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
    Recipe recipe;
    std::unordered_map<std::string, DynamicFeature> inputs;
    std::optional<RecipeAction> recipe_action;
};

} // namespace grunk