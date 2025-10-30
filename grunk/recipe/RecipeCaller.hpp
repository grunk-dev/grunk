#pragma once 

#include "grunk/dynamic/feature.hpp"

namespace grunk {

class Recipe;

class RecipeCaller
{
public:

    RecipeCaller(
        std::string const& name, 
        Feature<Recipe> const& recipe,
        lua_State* lua
    );

    struct Proxy {
        std::string key;
        RecipeCaller& rc;

        template <typename T>
        void operator=(T&& other) {
            if (!rc.locked()) {
                if constexpr (std::is_same_v<std::decay_t<T>, DynamicFeature>) {
                    rc.inputs.insert({key, std::forward<T>(other)});
                } else {
                    if constexpr (std::is_same_v<std::decay_t<T>, sol::object>) {
                        auto feature = DynamicFeature(std::forward<T>(other));
                        rc.inputs.insert({key, feature});
                    } else {
                        sol::state_view lua(rc.lua);
                        auto obj = sol::make_object(lua, std::forward<T>(other));
                        auto feature = DynamicFeature(obj);
                        rc.inputs.insert({key, feature});
                    }
                }
            } else {
                throw std::runtime_error(
                    "Cannot assign an input to a RecipeCaller once it has been locked. "
                    "A RecipeCaller gets locked once an output has been queried. "
                    "All inputs must be assigned before the first output is queried."
                );
            }
        }

        operator DynamicFeature();

        DynamicFeature to_feature();
    };

    Proxy operator[](std::string const& key);

    bool locked() const;

    DynamicFeature get(std::string const& key);

    RecipeCaller& with_id(std::string const& value);
    void set_id(std::string const& value);

private:
    void create_recipe_action();

    std::string name;
    std::optional<std::string> id;
    Feature<Recipe> source_recipe;
    lua_State* lua;
    std::unordered_map<std::string, DynamicFeature> inputs;
    std::optional<Feature<Recipe>> target_recipe;
};

} // namespace grunk