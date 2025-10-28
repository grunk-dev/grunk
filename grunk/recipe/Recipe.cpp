#include "grunk/recipe/Recipe.hpp"
#include "grunk/version.hpp"
#include "grunk/dynamic/internal/StringifiedTree.hpp"

#include <yaml-cpp/yaml.h>
#include <sol//sol.hpp>

namespace grunk {

    Recipe::Recipe(grunk::environment const& env)
     : environment(env)
    {}

    Recipe Recipe::clone() const
    {
        auto cloned_nodes = parametric::DAGNode::new_cloned_node_map();

        sol::state_view lua(m_environment.lua_state());
        sol::environment env(lua, sol::create, lua.globals());
        env[sol::metatable_key]["__index"] = m_environment[sol::metatable_key]["__index"];
        m_environment.for_each([&env, &cloned_nodes](sol::object const& key, sol::object const& value) {
            if (value.is<DynamicFeature>()) {
                DynamicFeature const& f = value;
                env[key] = DynamicFeature(f.clone(cloned_nodes));
            }
        });

        Recipe ret(env);
        
        //TODO: Clone recipes

        return ret;
    }

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

    void Recipe::populate_from_file(std::string const& filename)
    {
        YAML::Node yml = YAML::LoadFile(filename);
        populate_from_node(yml);
    }

    void Recipe::populate_from_string(std::string const& yml)
    {
        YAML::Node node = YAML::Load(yml);
        populate_from_node(node);
    }

    void Recipe::populate_from_node(YAML::Node const& yml)
    {
        for (auto const& kv : yml["parameters"]) {
            std::string key = kv.first.as<std::string>();
            std::string val = kv.second.as<std::string>();
            std::string val_f = details::ctor_syntax_to_new_feature_syntax(val);
            eval(key + " = " + val_f);
        }
        eval(yml["steps"].as<std::string>());
        tag_features();
    }

} // namespace grunk