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

// The uses: block only validates a required plugin's *name*, not its exact version
// (see docs/usage.rst's "Using grunk plugins" section) - two builds of a plugin sharing
// one name are often meant to be interchangeable, e.g. examples/cpp/cad_autodiff's
// "geoml"/"geoml_adolc" plugins, which report different versions under the same name
// specifically so the same recipe can be read back against either one. This must keep
// working even if the version check is ever tightened elsewhere by mistake.
TEST(RecipePlugin, populate_succeeds_when_plugin_version_differs_but_name_matches)
{
    grunk::state grunk;
    grunk.load_compiled_plugin(grunk::PluginInfo{"recipeplugin", "1.2.3"}, luaopen_recipeplugin);

    auto recipe = grunk.create_recipe();
    std::string yaml = recipe.to_string();
    ASSERT_NE(yaml.find("recipeplugin: 1.2.3"), std::string::npos);

    grunk::state other;
    other.load_compiled_plugin(grunk::PluginInfo{"recipeplugin", "9.9.9"}, luaopen_recipeplugin);

    auto recipe2 = other.create_recipe();
    EXPECT_NO_THROW(recipe2.populate_from_string(yaml));
}

namespace {

struct RecipePluginLength
{
    RecipePluginLength() = default;
    RecipePluginLength(double x_) : x(x_) {}
    double get_x() const { return x; }
    double x{0.};
};

} // anonymous namespace

// The uses:-block mechanism reads plugins() / lua["grunk"]["plugins"], which
// note_plugin populates identically for every plugin kind (see state.hpp) - a recipe
// must list a compiled-Lua plugin, a begin_plugin C++ plugin and a load_lua_plugin_script
// plugin exactly the same way, regardless of which mechanism registered each one.
TEST(RecipePlugin, uses_block_lists_plugins_regardless_of_kind)
{
    grunk::state grunk;
    grunk.load_compiled_plugin(grunk::PluginInfo{"compiledplugin", "1.0.0"}, luaopen_recipeplugin);

    auto ns = grunk.begin_plugin(grunk::PluginInfo{"cppplugin", "2.0.0"});
    ns.register_type<RecipePluginLength>("Length")
        .add_constructors([](double x) { return RecipePluginLength(x); })
        .add_member_function("get_x", &RecipePluginLength::get_x);

    grunk.load_lua_plugin_script(
        grunk::PluginInfo{"luaplugin", "3.0.0"},
        "function add_one(x) return x + 1 end"
    );

    auto recipe = grunk.create_recipe();
    std::string yaml = recipe.to_string();

    EXPECT_NE(yaml.find("compiledplugin: 1.0.0"), std::string::npos);
    EXPECT_NE(yaml.find("cppplugin: 2.0.0"), std::string::npos);
    EXPECT_NE(yaml.find("luaplugin: 3.0.0"), std::string::npos);
}

// A recipe using a begin_plugin-registered C++ type must round-trip through YAML the
// same way one using a compiled-Lua plugin already does (see
// roundtrip_through_yaml_preserves_computation above).
TEST(RecipePlugin, roundtrip_through_yaml_preserves_cpp_plugin_computation)
{
    grunk::state grunk;
    auto ns = grunk.begin_plugin(grunk::PluginInfo{"cppplugin", "2.0.0"});
    ns.register_type<RecipePluginLength>("Length")
        .add_constructors([](double x) { return RecipePluginLength(x); })
        .add_member_function("get_x", &RecipePluginLength::get_x);

    std::string yaml;
    {
        auto recipe = grunk.create_recipe();
        recipe.eval(R"(
            u = grunk.feature(3.)
            l = cppplugin.Length.new(u)
            x = cppplugin.Length.get_x(l)
        )");
        recipe.tag();
        yaml = recipe.to_string();
    }

    grunk::state other;
    auto other_ns = other.begin_plugin(grunk::PluginInfo{"cppplugin", "2.0.0"});
    other_ns.register_type<RecipePluginLength>("Length")
        .add_constructors([](double x) { return RecipePluginLength(x); })
        .add_member_function("get_x", &RecipePluginLength::get_x);

    auto recipe = other.create_recipe();
    recipe.populate_from_string(yaml);

    auto x = recipe.get_feature("x");
    EXPECT_NEAR(x.value().as<double>(), 3., 1e-10);

    auto u = recipe.get_feature("u");
    u.set_value(9.);
    EXPECT_NEAR(x.value().as<double>(), 9., 1e-10);
}
