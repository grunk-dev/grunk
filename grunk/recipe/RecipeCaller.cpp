#include "RecipeCaller.hpp"
#include "grunk/recipe/Recipe.hpp"
#include "grunk/recipe/RecipeAction.hpp"
#include "grunk/recipe/RecipeGetValAction.hpp"

namespace grunk {

RecipeCaller::RecipeCaller(
    std::string const& name, 
    Feature<Recipe> const& recipe,
    lua_State* lua
)
 : name(name)
 , source_recipe(recipe)
 , lua(lua)
 , target_recipe(std::nullopt)
{}

RecipeCaller::Proxy RecipeCaller::operator[](std::string const& key)
{
    return {key, *this};
}

bool RecipeCaller::locked() const
{
    return target_recipe.has_value();
}

void RecipeCaller::create_recipe_action()
{
    auto ptr = std::shared_ptr<RecipeAction>(new RecipeAction(name));
    target_recipe = parametric::compute(ptr, source_recipe, inputs);
    target_recipe.value().set_id(id.value_or(name));
}

DynamicFeature RecipeCaller::get(std::string const& key)
{
    return RecipeCaller::Proxy{key, *this}.to_feature();
}

RecipeCaller& RecipeCaller::with_id(std::string const& value) {
    set_id(value);
    return *this;
}

void RecipeCaller::set_id(std::string const& value) {
    id = value;
    if (locked()) {
        target_recipe->set_id(value);
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
