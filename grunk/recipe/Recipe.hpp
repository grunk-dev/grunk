// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/dynamic/environment.hpp"

namespace YAML {
    class Node;
    class Emitter;
}

namespace grunk {

    class state;
    class RecipeCaller;


    /**
     * @brief Represents a recipe in grunk.
     *
     * A Recipe is a specialized environment that can contain parameters,
     * and sub-recipes. Recipes can be serialized to YAML,
     * deep-cloned, populated from YAML files or strings.
     *
     * @ingroup recipe
     */
    class Recipe : public environment 
    {
        friend class grunk::state;
        
    public:

    /**
     * @brief copy constructor
     * @param other the copied-from object
     */
    Recipe(Recipe const& other);

    /**
     * @brief copy assignment operator
     * @param other the copied-from object
     */
    Recipe& operator=(Recipe const& other);

    /**
     * @brief move constructor
     * @param other the moved-from object
     */
    Recipe(Recipe&&);

    /**
     * @brief move assignment operator
     * @param other the moved-from object
     */
    Recipe& operator=(Recipe&& other); 

    /**
     * @brief Serialize the recipe to a YAML string.
     *
     * This produces a YAML representation of the recipe suitable for
     * persisting to disk or displaying to the user.
     *
     * @return std::string YAML-formatted representation of the recipe.
     */
    std::string to_string() const;

    /**
     * @brief Deep-clone this recipe.
     *
     * The clone will contain copies of all parameters and sub-recipes
     * so the result is independent from the original.
     *
     * @return Recipe A deep copy of this recipe.
     */
    Recipe clone() const;

    /**
     * @brief Populate this recipe from a YAML file.
     *
     * The file at @p filename is parsed as YAML and used to set
     * parameters and sub-recipes of this instance.
     *
     * @param filename Path to the YAML file to read.
     */
    void populate_from_file(std::string const& filename);
        
    /**
     * @brief Populate this recipe from a YAML string.
     *
     * @param yml A YAML document as a string.
     */
    void populate_from_string(std::string const& yml);

    /**
     * @brief Insert a sub-recipe into this recipe.
     *
     * The provided @p recipe will be moved into the internal collection
     * under the given @p name.
     *
     * @param name Key under which to store the sub-recipe.
     * @param recipe Recipe object to move into this recipe.
     */
    void insert_recipe(std::string const& name, Recipe&& recipe);

    /**
     * @brief Insert a module script into this recipe
     *
     * @param name name of the module script
     * @param script a Lua script as a string, representing the module script
     */
     void insert_module_script(std::string const& name, std::string const& script);

    /**
     * @brief Tag this recipe.
     *
     * This iterates over all keys in the recipes environment and sets the 
     * corresponding feature ids to be equal to the keys. This is a convenience
     * method to name all features in a recipe before writing it to YAML.
     */
    void tag();

        /**
         * @brief Container describing a sub-recipe entry.
         *
         * A SubRecipe bundles the stored recipe feature together with the
         * Lua state used for its bindings and a human-readable name.
         */
        struct SubRecipe {

            /// The key name of the sub-recipe in the parent recipe.
            std::string name;
            /// The recipe feature (parameter wrapper) stored for this entry.
            Feature<Recipe> recipe;
            /// Pointer to the Lua state used by this sub-recipe (may be null).
            lua_State* lua_state;

            /**
             * @brief returns a proxy object that can be used to evaluate
             * the subrecipe as a function
             */
            RecipeCaller operator()() const;
        };

    /**
     * @brief Retrieve a const-reference to a stored sub-recipe feature.
     *
     * Throws or asserts if the key does not exist (behavior follows
     * the underlying implementation of the class).
     *
     * @param key Key of the sub-recipe to retrieve.
     * @return Feature<Recipe> const& Reference to the stored feature.
     */
    Feature<Recipe> const& get_recipe(std::string const& key) const;

    /**
     * @brief Retrieve a mutable reference to a stored sub-recipe feature.
     *
     * @param key Key of the sub-recipe to retrieve.
     * @return Feature<Recipe>& Mutable reference to the stored feature.
     */
    Feature<Recipe>& get_recipe(std::string const& key);

    /// Map of sub-recipes stored by name.
    std::unordered_map<std::string, SubRecipe> recipes;

    /// Map of modules stored by name
    std::unordered_map<std::string, Feature<std::string>> module_scripts;

    private:
    /**
     * @brief Construct a recipe from an existing environment.
     *
     * This constructor initializes the recipe using the provided
     * environment as its base state.
     *
     * @param state The environment to base this recipe on.
     */
    Recipe(grunk::environment const& state);

    /**
     * @brief Emit YAML representation to the provided emitter.
     *
     * This is an internal helper that writes the recipe's YAML
     * representation into @p out.
     *
     * @param out YAML emitter to write into.
     */
    void emit_yml(YAML::Emitter& out) const;

    /**
     * @brief Populate this recipe from a YAML node.
     *
     * This function is called by higher-level population helpers to
     * interpret a parsed YAML node and set parameters accordingly.
     *
     * @param node Parsed YAML node to read from.
     */
    void populate_from_node(YAML::Node const& node);

        /**
         * @brief Re-register the __index metamethod for the Lua "recipes" table.
         *
         * When a Recipe is copied or moved, the previously-registered Lua
         * metamethods may capture the old 'this' pointer. Call this to rebind
         * the __index metamethod so Lua code continues to access the correct
         * Recipe instance.
         */
        void re_register_lua_index();
    };

} // namespace grunk