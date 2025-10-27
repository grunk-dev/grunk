#include "grunk/recipe/Recipe.hpp"
#include "../version.hpp"
#include "grunk/dynamic/internal/StringifiedTree.hpp"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace grunk {

    Recipe::Recipe(grunk::state const& state)
     : environment(state.create_parametric_env())
    {}

    std::string Recipe::to_string() const
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        
        std::map<std::string, std::string> uses;
        uses["grunk"] = grunk_VERSION;

        out << YAML::Key << "uses" << YAML::Value << uses;

        StringifiedTree tree;
        m_environment.for_each([&tree](sol::object key, sol::object value) {
            if (value.is<DynamicFeature>()) {
                tree.parse(value.as<DynamicFeature>());
            }
        });

        out << YAML::Key << "parameters"
            << YAML::Value << tree.get_parameters()
            << YAML::Key << "steps" 
            << YAML::Value << YAML::Literal << tree.get_string()
            << YAML::EndMap;

        return out.c_str();
    }

    void Recipe::write(std::string const& filename) const
    {
        std::ofstream fout(filename);
        fout << to_string() << "\n";
    }

    Recipe Recipe::from_string(grunk::state const& state, std::string const& str)
    {
        auto recipe = Recipe(state);
        YAML::Node yml = YAML::Load(str);
        for (auto const& kv : yml["parameters"]) {
            std::string key = kv.first.as<std::string>();
            std::string val = kv.second.as<std::string>();
            recipe.eval(key + " = grunk.feature(" + val + ")");
        }
        recipe.eval(yml["steps"].as<std::string>());
        recipe.tag_features();
        return recipe;
    }

} // namespace grunk