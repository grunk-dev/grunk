#include "RecipeCaller.hpp"

namespace grunk {

RecipeCaller::RecipeCaller(std::string const& name, Recipe const& recipe)
 : name(name)
 , recipe(recipe)
 , recipe_action(std::nullopt)
{}

bool RecipeCaller::locked() const
{
    return recipe_action.has_value();
}

void RecipeCaller::create_recipe_action()
{

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