// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/dynamic/DynamicFeature.hpp"

namespace grunk {

class Recipe;


/**
 * @brief Helper class used to call/execute a Recipe from Lua or C++.
 *
 * RecipeCaller collects input DynamicFeatures (via a Proxy), optionally
 * assigns an id, and lazily creates the RecipeAction that performs the
 * execution. Once an output has been requested the caller becomes locked
 * and further input assignments are prohibited.
 *
 * @ingroup advanced_recipe
 */
class RecipeCaller
{
public:

    /**
     * @brief Create a RecipeCaller bound to a recipe and a Lua state.
     *
     * @param name A human-readable name for the caller/action.
     * @param recipe The source recipe feature to execute.
     * @param lua Pointer to the Lua state used for value conversions.
     */
    RecipeCaller(
        std::string const& name, 
        Feature<Recipe> const& recipe,
        lua_State* lua
    );

    /**
     * @brief Proxy object used by operator[] to assign inputs.
     *
     * The Proxy stores the key and a reference back to the owning
     * RecipeCaller. Assigning various types to the proxy will convert or
     * wrap them into a DynamicFeature and insert them into the caller's
     * input map. Assignment is disallowed once the caller is locked.
     *
     * @ingroup advanced_recipe
     */
    struct Proxy {
        std::string key;
        RecipeCaller& rc;

        /**
         * @brief Assign a value to the proxied input key.
         *
         * The template overload accepts either an existing
         * DynamicFeature, a sol::object, or any other type that can be
         * converted into a Lua object; it will be wrapped into a
         * DynamicFeature and stored. Throws if the caller is locked.
         */
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
                        sol::state_view state_view(rc.lua);
                        auto obj = sol::make_object(state_view, std::forward<T>(other));
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

        /**
         * @brief Convert the proxy to a DynamicFeature.
         *
         * This operator triggers conversion/lookup of the stored input
         * feature and is used when retrieving the assigned value.
         */
        operator DynamicFeature();

        /**
         * @brief Explicitly convert the proxied value to a feature.
         *
         * @return The DynamicFeature associated with this proxy's key.
         */
        DynamicFeature to_feature();
    };

    /**
     * @brief Access a proxy for a named input key.
     *
     * Example usage from Lua: caller["input_name"] = some_value
     *
     * @param key Name of the input to access.
     * @return A Proxy object bound to the provided key.
     */
    Proxy operator[](std::string const& key);

    /**
     * @brief Whether the caller is locked (no further input assignments).
     *
     * The caller is locked once an output has been requested; after that
     * input assignment via Proxy::operator= will throw.
     */
    bool locked() const;

    /**
     * @brief Retrieve a previously assigned input by key.
     *
     * @param key The input name.
     * @return The DynamicFeature assigned to the key.
     */
    DynamicFeature get(std::string const& key);

    /**
     * @brief Fluent setter for the optional id field.
     *
     * @param value Identifier string to associate with the call.
     * @return Reference to *this for chaining.
     */
    RecipeCaller& with_id(std::string const& value);

    /**
     * @brief Set the id for this caller.
     *
     * @param value Identifier value.
     */
    void set_id(std::string const& value);

private:
    /**
     * @brief Lazily create the underlying RecipeAction and wire inputs.
     */
    void create_recipe_action();

    /**
     * @brief Name used for identification/serialization.
     */
    std::string name;

    /**
     * @brief Optional identifier provided by the caller.
     */
    std::optional<std::string> id;

    /**
     * @brief Source recipe feature that will be executed.
     */
    Feature<Recipe> source_recipe;

    /**
     * @brief Lua state pointer used for conversions to sol::object.
     */
    lua_State* lua;

    /**
     * @brief Map of named inputs assigned prior to invocation.
     */
    std::unordered_map<std::string, DynamicFeature> inputs;

    /**
     * @brief Optionally holds the target recipe feature created by the action.
     */
    std::optional<Feature<Recipe>> target_recipe;
};

} // namespace grunk