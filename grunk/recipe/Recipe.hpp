#pragma once 

#include "grunk/dynamic/environment.hpp"

namespace YAML {
    class Node;
    class Emitter;
}

namespace grunk {

    class state;
    class RecipeCaller;

    class Recipe : public environment 
    {
        friend grunk::state;
        
    public:

        /**
         * @brief serializes a recipe to yaml. This is used to write grunk recipes to file
         * 
         * @return YAML::Node A YAML::Node instance from the yaml-cpp library storing a yaml representation of the recipe
         */
        std::string to_string() const;

        Recipe clone() const;

        void populate_from_file(std::string const& filename);
        
        void populate_from_string(std::string const& yml);

        void insert_recipe(std::string const& name, Recipe&& recipe);

        void tag();

        struct SubRecipe {

            std::string name;
            Feature<Recipe> recipe;
            lua_State* lua_state;

            RecipeCaller operator()() const;
        };

        Feature<Recipe> const& get_recipe(std::string const& key) const;
        Feature<Recipe>& get_recipe(std::string const& key);

        std::unordered_map<std::string, SubRecipe> recipes;

    private:
        Recipe(grunk::environment const& state);

        void emit_yml(YAML::Emitter& out) const;

        void populate_from_node(YAML::Node const& node);
    };

} // namespace grunk