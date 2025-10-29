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

        Feature<Recipe> const& get_recipe(std::string const&) const;
        Feature<Recipe>& get_recipe(std::string const&);

        void insert_recipe(std::string const& name, Recipe&& recipe);

        RecipeCaller recipe_caller(std::string const& recipe_name) const;

    private:
        Recipe(grunk::environment const& state);

        void emit_yml(YAML::Emitter& out) const;

        void populate_from_node(YAML::Node const& node);

        std::unordered_map<std::string, Feature<Recipe>> recipes;
    };

} // namespace grunk