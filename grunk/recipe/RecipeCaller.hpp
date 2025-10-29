#pragma once 

#include "grunk/dynamic/feature.hpp"

namespace grunk {

class Recipe;

class RecipeCaller
{
public:

    RecipeCaller(std::string const& name, Feature<Recipe> const& recipe);

    struct Proxy {
        std::string key;
        RecipeCaller& rc;

        // this is an alias for operator=(DynamicFeature const&). I keep
        // it here because in LUA I cannot overload assignment and we have 
        // to use this function
        void assign(DynamicFeature const& other);

        void operator=(DynamicFeature const& other);

        operator DynamicFeature();

        DynamicFeature to_feature();
    };

    Proxy operator[](std::string const& key);

    bool locked() const;

    DynamicFeature get(std::string const& key);

private:
    void create_recipe_action();

    std::string name;
    Feature<Recipe> source_recipe;
    std::unordered_map<std::string, DynamicFeature> inputs;
    std::optional<Feature<Recipe>> target_recipe;
};

} // namespace grunk