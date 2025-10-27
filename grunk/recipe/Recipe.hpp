#pragma once 

#include "grunk/dynamic/environment.hpp"
#include "grunk/dynamic/state.hpp"

namespace grunk {

    class Recipe : public environment 
    {
    public:
        Recipe(grunk::state const& state);

        /**
         * @brief serializes a recipe to yaml. This is used to write grunk recipes to file
         * 
         * @return YAML::Node A YAML::Node instance from the yaml-cpp library storing a yaml representation of the recipe
         */
        std::string to_string() const;

        void write(std::string const& filename) const;

        static Recipe from_string(grunk::state const& state, std::string const& str);

    private:
    };

} // namespace grunk