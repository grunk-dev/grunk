// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include "grunk/recipe/Recipe.hpp"
#include "grunk/version.hpp"
#include "grunk/dynamic/Serializer.hpp"
#include "grunk/recipe/RecipeCaller.hpp"
#include "grunk/recipe/ModuleAction.hpp"

#include <yaml-cpp/yaml.h>
#include <sol//sol.hpp>

namespace grunk {

    Recipe::Recipe(grunk::environment const& env)
     : environment(env)
    {
        m_environment.create_named("recipes");
        re_register_lua_index();
    }

    Recipe::Recipe(Recipe&& other)
    : environment(std::move(other))
    , recipes(std::move(other.recipes))
    {
        re_register_lua_index();  // ← rebind __index to new 'this' after every move
    }

    Recipe::Recipe(Recipe const& other)
        : environment(other)
        , recipes(other.recipes)
    {
        re_register_lua_index();  // ← same for copy constructor
    }

    Recipe& Recipe::operator=(Recipe&& other)
    {
        if (this != &other) {
            environment::operator=(std::move(other));
            recipes = std::move(other.recipes);
            re_register_lua_index();
        }
        return *this;
    }

    Recipe& Recipe::operator=(Recipe const& other)
    {
        if (this != &other) {
            environment::operator=(other);
            recipes = other.recipes;
            re_register_lua_index();
        }
        return *this;
    }

    void Recipe::re_register_lua_index()
    {
        sol::state_view lua(m_environment.lua_state());
        sol::table mt = lua.create_table();
        mt.set_function("__index", [this, lua](sol::table, std::string const& key) -> sol::object {
            return sol::make_object(lua, this->recipes[key]);
        });
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

        for (auto const& [key, script] : module_scripts) {
            Feature<std::string> cloned_script = script.clone(cloned_nodes);
            ret.module_scripts.insert({key, cloned_script});
            ret.install_module_proxy(key, cloned_script);
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
        if (yml["uses"] && yml["uses"].IsMap()) {
            sol::state_view lua(m_environment.lua_state());
            sol::object plugins_obj = lua["grunk"]["plugins"];
            sol::table plugins_table = (plugins_obj.valid() && plugins_obj.is<sol::table>())
                ? plugins_obj.as<sol::table>()
                : lua.create_table();

            for (auto const& kv : yml["uses"]) {
                std::string key = kv.first.as<std::string>();
                if (key == "grunk") {
                    continue;
                }
                std::string version = kv.second.as<std::string>();

                sol::object loaded = plugins_table[key];
                if (!loaded.valid() || !loaded.is<std::string>()) {
                    throw io_error(
                        "Recipe requires plugin \"" + key + "\" (version " + version +
                        "), which is not loaded in this grunk::state."
                    );
                }
            }
        }
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
        if (yml["modules"]) {
            for (auto const& kv : yml["modules"]) {
                std::string key = kv.first.as<std::string>();
                std::string val = kv.second.as<std::string>();
                insert_module_script(key, val);
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

        sol::state_view lua(m_environment.lua_state());
        sol::object plugins_obj = lua["grunk"]["plugins"];
        if (plugins_obj.valid() && plugins_obj.is<sol::table>()) {
            for (auto const& kv : plugins_obj.as<sol::table>()) {
                if (kv.first.is<std::string>() && kv.second.is<std::string>()) {
                    uses[kv.first.as<std::string>()] = kv.second.as<std::string>();
                }
            }
        }

        out << YAML::Key << "uses" << YAML::Value << uses;

        Serializer tree;
        m_environment.for_each([&tree](sol::object key, sol::object value) {
            if (value.is<DynamicFeature>()) {
                tree.parse(value.as<DynamicFeature>());
            }
        });

        if (tree.get_parameters().size() > 0) {
            out << YAML::Key << "parameters"
                << YAML::Value << tree.get_parameters();
        }

        if (module_scripts.size() > 0) {
            out << YAML::Key << "modules" << YAML::BeginMap;
            for (auto const& [key, script] : module_scripts) {
                out << YAML::Key << key
                    << YAML::Value << YAML::Literal << script.value();
            }
            out << YAML::EndMap;
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

    void Recipe::insert_module_script(std::string const& name, std::string const& script)
    {
        Feature<std::string> feature = grunk::feature(script);
        module_scripts[name] = feature;
        install_module_proxy(name, feature);
    }

    void Recipe::install_module_proxy(std::string const& name, Feature<std::string> const& script)
    {
        lua_State* lua_state = m_environment.lua_state();
        sol::state_view lua(lua_state);

        sol::table proxy_mt = lua.create_table();
        proxy_mt.set_function(
            "__index",
            [lua_state, name, script](sol::table, std::string const& function_name) -> sol::object {
                sol::state_view lua(lua_state);
                return sol::make_object(
                    lua,
                    sol::as_function(
                        [lua_state, name, function_name, script](sol::variadic_args va) -> DynamicFeature {
                            auto raw_args = std::vector<sol::object>(va.begin(), va.end());
                            std::vector<DynamicFeature> args;
                            args.reserve(raw_args.size());
                            for (auto const& raw : raw_args) {
                                grunk::object obj = raw;
                                if (obj.is<DynamicFeature>()) {
                                    args.push_back(obj.as<DynamicFeature>());
                                } else {
                                    args.push_back(grunk::feature(obj));
                                }
                            }
                            return details::ModuleActionFactory::new_action(
                                name, function_name, lua_state, script, args
                            ).output();
                        }
                    )
                );
            }
        );

        sol::table proxy = lua.create_table();
        proxy[sol::metatable_key] = proxy_mt;
        m_environment[name] = proxy;
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
