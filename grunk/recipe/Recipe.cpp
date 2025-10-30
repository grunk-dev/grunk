#include "grunk/recipe/Recipe.hpp"
#include "grunk/version.hpp"
#include "grunk/dynamic/internal/StringifiedTree.hpp"
#include "grunk/recipe/RecipeCaller.hpp"

#include <yaml-cpp/yaml.h>
#include <sol//sol.hpp>

namespace grunk {

    Recipe::Recipe(grunk::environment const& env)
     : environment(env)
    {
        sol::state_view lua(m_environment.lua_state());
        sol::table mt = lua.create_table();
        mt.set_function("__index", [=](sol::table ts, std::string const& key) -> sol::object {
            return sol::make_object(lua, recipes[key]);
        });
        m_environment.create_named("recipes");
        sol::table lua_recipes = m_environment["recipes"];
        lua_recipes[sol::metatable_key] = mt;
        
    }

    Recipe Recipe::clone() const
    {
        auto cloned_nodes = parametric::DAGNode::new_cloned_node_map();

        sol::state_view lua(m_environment.lua_state());
        sol::environment env(lua, sol::create, lua.globals());
        env[sol::metatable_key]["__index"] = m_environment[sol::metatable_key]["__index"];
        Recipe ret(env);

        m_environment.for_each([&ret, &cloned_nodes](sol::object const& key, sol::object const& value) {
            if (value.is<DynamicFeature>()) {
                std::string const& id = key.as<std::string const&>();
                DynamicFeature const& f = value.as<DynamicFeature const&>();
                ret[id] = f.clone(cloned_nodes);
            }
        });

        for (auto const& [key, value] : recipes) {
            Recipe recipe = value.recipe.value().clone();
            ret.recipes.insert(
                {
                    key, 
                    SubRecipe{key, grunk::feature<Recipe>(std::move(recipe)), m_environment.lua_state()}
                }
            );
        }

        return ret;
    }

    std::string Recipe::to_string() const
    {
        YAML::Emitter out;
        emit_yml(out);
        std::string ret = out.c_str();
        if (ret.back() != '\n') { ret += '\n'; }
        return ret;
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
        if (yml["recipes"]) {
            for (auto const& kv : yml["recipes"]) {
                std::string key = kv.first.as<std::string>();

                sol::state_view lua(m_environment.lua_state());
                sol::environment env(lua, sol::create, lua.globals());
                env[sol::metatable_key]["__index"] = m_environment[sol::metatable_key]["__index"];
                auto recipe = Recipe(env);
                recipe.populate_from_node(kv.second);
                recipes.insert(
                    {
                        key, 
                        SubRecipe{key, Feature<Recipe>(std::move(recipe)), m_environment.lua_state()}
                    }
                );
            }
        }
        if (yml["parameters"]) {
            for (auto const& kv : yml["parameters"]) {
                std::string key = kv.first.as<std::string>();
                std::string val = kv.second.as<std::string>();
                std::string val_f = details::ctor_syntax_to_new_feature_syntax(val);
                eval(key + " = " + val_f);
            }
        }
        if (yml["steps"]) {
            eval(yml["steps"].as<std::string>());
        }
        tag();
    }

    void Recipe::emit_yml(YAML::Emitter& out) const
    {
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

        if (tree.get_parameters().size() > 0) {
            out << YAML::Key << "parameters"
                << YAML::Value << tree.get_parameters();
        }

        if (!tree.get_string().empty()) {
            out << YAML::Key << "steps" 
                << YAML::Value << YAML::Literal << tree.get_string();
        }
            
        if (recipes.size() > 0) {
            out << YAML::Key << "recipes";
            out << YAML::Value << YAML::BeginMap;
            for (auto const& [key, subrecipe] : recipes) {
                out << YAML::Key << key
                    << YAML::Value;
                subrecipe.recipe.value().emit_yml(out);
            }
            out << YAML::EndMap;
        }

        out << YAML::EndMap;
    }

    void Recipe::insert_recipe(std::string const& name, Recipe&& recipe)
    {
        recipes.insert(
            { 
                name, 
                SubRecipe{name, grunk::feature<Recipe>(recipe), m_environment.lua_state()}
            }
        );
    }

    void Recipe::tag() 
    {
        tag_features();

        // tag RecipeCallers
        m_environment.for_each([](sol::object key, sol::object value) {
            if (key.is<std::string>() && value.is<RecipeCaller>()) {
                value.as<RecipeCaller&>().set_id(key.as<std::string const&>());
            }
        });
    }

    Feature<Recipe> const& Recipe::get_recipe(std::string const& key) const
    {
        return recipes.at(key).recipe;
    }
    
    Feature<Recipe>& Recipe::get_recipe(std::string const& key)
    {
        return recipes.at(key).recipe;
    }

    RecipeCaller Recipe::SubRecipe::operator()() const
    {
        return RecipeCaller(name, recipe, lua_state);
    }

} // namespace grunk
