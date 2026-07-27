// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
#include <grunk/recipe.hpp>

namespace {

// Same tiny stand-in for a compiled Lua C extension used in dynamic_plugin.cpp: a
// plain lua_CFunction exposing one free function, built with sol2 but otherwise
// unaware of grunk.
int luaopen_recipeplugin(lua_State* L)
{
    sol::state_view lua(L);
    sol::table mod = lua.create_table();
    mod.set_function("double_it", [](double x) { return 2. * x; });
    sol::stack::push(L, mod);
    return 1;
}

} // anonymous namespace

TEST(RecipePlugin, uses_block_lists_loaded_plugins)
{
    grunk::state grunk;
    grunk.load_compiled_plugin(grunk::PluginInfo{"recipeplugin", "1.2.3"}, luaopen_recipeplugin);

    auto recipe = grunk.create_recipe();
    std::string out = "\n" + recipe.to_string();
    std::string expected = R"(
uses:
  grunk: )" grunk_VERSION R"(
  recipeplugin: 1.2.3
)";
    EXPECT_EQ(out, expected);
}

TEST(RecipePlugin, roundtrip_through_yaml_preserves_computation)
{
    grunk::state grunk;
    grunk.load_compiled_plugin(grunk::PluginInfo{"recipeplugin", "1.2.3"}, luaopen_recipeplugin);

    std::string yaml;
    {
        auto x = grunk.feature(3.).with_id("x");
        auto recipe = grunk.create_recipe();
        recipe["x"] = x;
        recipe.eval("y = recipeplugin.double_it(x)");
        recipe.tag();
        yaml = recipe.to_string();
    }

    ASSERT_NE(yaml.find("uses:"), std::string::npos);
    ASSERT_NE(yaml.find("recipeplugin: 1.2.3"), std::string::npos);

    // A fresh state, with the same plugin loaded, must be able to read the recipe
    // back and reproduce the computation, including reactive invalidation.
    grunk::state other;
    other.load_compiled_plugin(grunk::PluginInfo{"recipeplugin", "1.2.3"}, luaopen_recipeplugin);

    auto recipe = other.create_recipe();
    recipe.populate_from_string(yaml);

    auto y = recipe.get_feature("y");
    EXPECT_NEAR(y.value().as<double>(), 6., 1e-10);

    auto x = recipe.get_feature("x");
    x.set_value(5.);
    EXPECT_NEAR(y.value().as<double>(), 10., 1e-10);
}

TEST(RecipePlugin, populate_fails_clearly_when_required_plugin_is_missing)
{
    grunk::state grunk;
    grunk.load_compiled_plugin(grunk::PluginInfo{"recipeplugin", "1.2.3"}, luaopen_recipeplugin);

    auto recipe = grunk.create_recipe();
    std::string yaml = recipe.to_string();
    ASSERT_NE(yaml.find("recipeplugin: 1.2.3"), std::string::npos);

    // A state that never loaded the plugin must refuse to populate a recipe that
    // declares it under "uses", rather than failing later with a confusing
    // "symbol not found" error the first time steps referencing it are evaluated.
    grunk::state other;
    auto recipe2 = other.create_recipe();
    EXPECT_THROW(recipe2.populate_from_string(yaml), grunk::io_error);
}
