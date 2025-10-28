#include "RecipeCaller.hpp"

namespace grunk {

RecipeCaller::RecipeCaller(std::string const& name, DynamicFeature const& recipe)
 : name(name)
 , source_recipe(recipe)
 , target_recipe(std::nullopt)
{}

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

RecipeCaller::Proxy::operator DynamicFeature() {
    if (!rc.locked()) {
        rc.create_recipe_action();
    }
}

} // namespace grunk