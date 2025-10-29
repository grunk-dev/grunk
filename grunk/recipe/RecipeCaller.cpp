#include "RecipeCaller.hpp"
#include "grunk/recipe/Recipe.hpp"
#include "grunk/recipe/RecipeAction.hpp"
#include "grunk/recipe/RecipeGetValAction.hpp"

namespace grunk {

RecipeCaller::RecipeCaller(std::string const& name, Feature<Recipe> const& recipe)
 : name(name)
 , source_recipe(recipe)
 , target_recipe(std::nullopt)
{}

RecipeCaller::Proxy RecipeCaller::operator[](std::string const& key)
{
    return RecipeCaller::Proxy{key, *this};
}

bool RecipeCaller::locked() const
{
    return target_recipe.has_value();
}

void RecipeCaller::create_recipe_action()
{
    auto ptr = std::shared_ptr<RecipeAction>(new RecipeAction(name));
    target_recipe = parametric::compute(ptr, source_recipe, inputs);
}

void RecipeCaller::Proxy::operator=(DynamicFeature const& other)
{
    if (!rc.locked()) {
        rc.inputs.insert({key, other});
    } else {
        throw std::runtime_error(
            "Cannot assign an input to a RecipeCaller once it has been locked. "
            "A RecipeCaller gets locked once an output has been queried. "
            "All inputs must be assigned before the first output is queried."
        );
    }
}

DynamicFeature RecipeCaller::Proxy::to_feature() {
    if (!rc.locked()) {
        rc.create_recipe_action();
    }
    auto ptr = std::shared_ptr<RecipeGetValAction>(new RecipeGetValAction(rc.name));
    return parametric::compute(ptr, *rc.target_recipe, key);
}

RecipeCaller::Proxy::operator DynamicFeature() {
    return to_feature();
}

} // namespace grunk