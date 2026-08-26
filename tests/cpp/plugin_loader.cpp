// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
#include <grunk/plugin.hpp>
#include <fstream>
#include <cstdio>

// NATIVE_FIXTURE_SO_PATH is set by CMakeLists.txt to the built path of the
// plugin_native_fixture shared library target (see plugin_native_fixture.cpp).

TEST(PluginLoader, load_native_registers_plugin_from_shared_library)
{
    grunk::state grunk;

    grunk::PluginInfo info = grunk::plugin::load_native(grunk, NATIVE_FIXTURE_SO_PATH);

    EXPECT_EQ(info.name, "native_fixture");
    EXPECT_EQ(info.version, "1.0.0");

    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].name, "native_fixture");
    EXPECT_EQ(grunk.plugins()[0].version, "1.0.0");

    auto penv = grunk.create_parametric_env();
    auto res = penv.eval(R"(
        u = grunk.feature(3.)
        f = native_fixture.FixtureType.new(u)
        x = native_fixture.twice(f)
    )");
    ASSERT_TRUE(res.valid());

    auto fx = penv.get_feature("x");
    EXPECT_NEAR(fx.value().as<double>(), 6.0, 1e-15);

    auto fu = penv.get_feature("u");
    fu.set_value(5.0);
    EXPECT_NEAR(fx.value().as<double>(), 10.0, 1e-15);
}

TEST(PluginLoader, load_native_missing_file_throws)
{
    grunk::state grunk;
    EXPECT_THROW(grunk::plugin::load_native(grunk, "does_not_exist.so"), std::runtime_error);
}

TEST(PluginLoader, load_script_records_metadata_and_registers_namespace)
{
    grunk::state grunk;

    std::string filename = "grunk_test_loader_plugin.lua";
    {
        std::ofstream fout(filename);
        fout << "function add_one(x) return x + 1 end";
    }

    grunk::PluginInfo info{"loader_lua_plugin", "1.0"};
    grunk::plugin::load_script(grunk, info, filename);
    std::remove(filename.c_str());

    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].name, "loader_lua_plugin");

    auto env = grunk.create_env();
    auto res = env.eval("y = loader_lua_plugin.add_one(1)");
    ASSERT_TRUE(res.valid());
    EXPECT_EQ(env.get("y").as<int>(), 2);
}
