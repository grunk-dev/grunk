// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
#include <fstream>
#include <cstdio>

TEST(PluginLuaScript, records_metadata_and_registers_namespace)
{
    grunk::state grunk;

    grunk::PluginInfo info{"lua_plugin", "0.1.0"};
    grunk.load_lua_plugin_script(info, "function add_one(x) return x + 1 end");

    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].name, "lua_plugin");
    EXPECT_EQ(grunk.plugins()[0].version, "0.1.0");

    auto env = grunk.create_env();
    auto res = env.eval("y = lua_plugin.add_one(41)");
    ASSERT_TRUE(res.valid());
    EXPECT_EQ(env.get("y").as<int>(), 42);
}

// Functions defined by a Lua-script plugin must decorate exactly like a compiled
// plugin's free functions or a begin_plugin-registered C++ function - dependency
// tracking, lazy evaluation, invalidation - through a parametric environment.
TEST(PluginLuaScript, functions_are_tracked_through_parametric_env)
{
    grunk::state grunk;

    grunk.load_lua_plugin_script(
        grunk::PluginInfo{"lua_plugin", "0.1.0"},
        "function twice(x) return 2 * x end"
    );

    auto penv = grunk.create_parametric_env();
    auto res = penv.eval(R"(
        u = grunk.feature(3.)
        x = lua_plugin.twice(u)
    )");
    ASSERT_TRUE(res.valid());

    auto fx = penv.get_feature("x");
    EXPECT_NEAR(fx.value().as<double>(), 6.0, 1e-15);

    auto fu = penv.get_feature("u");
    fu.set_value(5.0);
    EXPECT_NEAR(fx.value().as<double>(), 10.0, 1e-15);
}

TEST(PluginLuaScript, load_from_file_records_metadata)
{
    grunk::state grunk;

    std::string filename = "grunk_test_lua_plugin.lua";
    {
        std::ofstream fout(filename);
        fout << "function add_one(x) return x + 1 end";
    }

    grunk::PluginInfo info{"lua_file_plugin", "2.0.0"};
    grunk.load_lua_plugin_file(info, filename);
    std::remove(filename.c_str());

    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].name, "lua_file_plugin");
    EXPECT_EQ(grunk.plugins()[0].version, "2.0.0");

    auto env = grunk.create_env();
    auto res = env.eval("y = lua_file_plugin.add_one(10)");
    ASSERT_TRUE(res.valid());
    EXPECT_EQ(env.get("y").as<int>(), 11);
}

TEST(PluginLuaScript, load_from_missing_file_throws)
{
    grunk::state grunk;
    EXPECT_THROW(
        grunk.load_lua_plugin_file(grunk::PluginInfo{"lua_file_plugin", "1.0"}, "does_not_exist.lua"),
        grunk::io_error
    );
    // A failed load must not record a plugin that was never actually populated.
    EXPECT_EQ(grunk.plugins().size(), 0u);
}
