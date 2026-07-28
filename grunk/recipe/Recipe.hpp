// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/dynamic/environment.hpp"

#include <map>

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
     * @brief Insert a module script into this recipe.
     *
     * The script is stored as a Feature<std::string> so that it participates in the
     * recipe's deep-clone semantics, and installs a callable Lua proxy under @p name in
     * this recipe's environment. Calling a function through that proxy (e.g. `name.foo(...)`
     * from a `steps:` script) creates a ModuleAction that depends on the stored script
     * feature: editing the script's value invalidates and recomputes every call made into
     * this module.
     *
     * @param name name under which the module is accessible from `steps:`
     * @param script a Lua script as a string, defining the functions of the module
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
     * @brief Mark a feature of this recipe as a named output.
     *
     * Stores the mapping under @p name in the #outputs map, so it is written
     * to and read back from the recipe's `outputs:` YAML block. @p feature_id
     * must currently resolve to a `DynamicFeature` in this recipe's
     * environment (see environment::get_feature); an id that resolves to
     * nothing, or to an anonymous/unemittable node, cannot be looked up again
     * after a YAML round-trip, so this throws grunk::io_error in that case.
     *
     * @param name Output name, used as the key of the #outputs map.
     * @param feature_id Id of the feature this output designates.
     */
    void insert_output(std::string const& name, std::string const& feature_id);

    /**
     * @brief Retrieve a feature previously marked as an output.
     *
     * @param name Output name, as passed to insert_output.
     * @return DynamicFeature The feature designated by this output.
     */
    DynamicFeature get_output(std::string const& name) const;

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

    /// Map of module scripts stored by name.
    std::unordered_map<std::string, Feature<std::string>> module_scripts;

    /// Map of output names to the id of the feature they designate.
    std::map<std::string, std::string> outputs;

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
     * @brief Install a callable Lua proxy for a module in this recipe's environment.
     *
     * Sets `m_environment[name]` to a table whose `__index` metamethod lazily creates a
     * ModuleAction bound to @p script for every function name accessed on it. This does not
     * touch `module_scripts`; callers are responsible for keeping that map in sync (see
     * insert_module_script and clone).
     *
     * @param name name under which the module is accessible from `steps:`
     * @param script the module's script feature the created ModuleActions will depend on
     */
    void install_module_proxy(std::string const& name, Feature<std::string> const& script);

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