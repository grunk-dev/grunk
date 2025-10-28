#pragma once 

#include "grunk/dynamic/environment.hpp"

namespace YAML {
    class Node;
}

namespace grunk {

    class state;

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

        void populate_from_file(std::string const& filename);
        void populate_from_string(std::string const& yml);

    private:
        Recipe(grunk::environment const& state);

        void populate_from_node(YAML::Node const& node);
    };

} // namespace grunk