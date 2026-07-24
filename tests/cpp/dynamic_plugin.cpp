// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>

namespace {

// A tiny stand-in for a compiled Lua C extension (e.g. a SWIG-Lua module): a plain
// C function matching the lua_CFunction/luaopen_* signature, built with sol2 for
// convenience but otherwise unaware of grunk. Mirrors the shape swig-adol-c's
// adtl.so has that actually matters for load_compiled_plugin/register_external_type:
// - a free function stored directly on the module table (like adtl.tan)
// - a class ("Box") whose constructor lives behind its own table's __call
//   metamethod rather than a plain "new" entry, and whose instance methods are
//   only reachable through the instance's own metatable - not through the module
//   table at all.
struct Box
{
    Box() = default;
    Box(double x_) : x(x_) {}
    void set_x(double x_) { x = x_; }
    double get_x() const { return x; }
    double x{0.};
};

int luaopen_testplugin(lua_State* L)
{
    sol::state_view lua(L);
    sol::table mod = lua.create_table();

    mod.set_function("double_it", [](double x) { return 2. * x; });

    lua.new_usertype<Box>("__dynamic_plugin_test_Box",
        sol::no_constructor,
        "set_x", &Box::set_x,
        "get_x", &Box::get_x
    );

    sol::table box_static = lua.create_table();
    sol::table box_meta = lua.create_table();
    box_meta.set_function("__call", sol::overload(
    [](sol::table) { return Box(); },
    [](sol::table, double x) { return Box(x); }
));
    box_static[sol::metatable_key] = box_meta;
    mod["Box"] = box_static;

    sol::stack::push(L, mod);
    return 1;
}

} // anonymous namespace

TEST(plugin, load_compiled_plugin_records_metadata)
{
    grunk::state grunk;

    grunk::PluginInfo info{"testplugin", "1.2.3"};
    grunk.load_compiled_plugin(info, luaopen_testplugin);

    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].name, "testplugin");
    EXPECT_EQ(grunk.plugins()[0].version, "1.2.3");
}

TEST(plugin, compiled_plugin_free_functions_are_tracked)
{
    grunk::state grunk;

    grunk::PluginInfo info{"testplugin", "1.0"};
    grunk.load_compiled_plugin(info, luaopen_testplugin);

    auto x = grunk.feature(3.);
    auto y = grunk.action("testplugin.double_it", x);
    EXPECT_NEAR(y.value().as<double>(), 6., 1e-15);

    x.set_value(5.);
    EXPECT_NEAR(y.value().as<double>(), 10., 1e-15);
}

// This is the scenario create_decorated_environment's recursive decoration exists for:
// a class nested inside a plugin's own namespace table must still be tracked - both
// its constructor and its methods - exactly as a flat register_type registration would.
//
// Note there is no .add_member_function call for "get_x" here: SWIG-Lua bindings give
// no way to enumerate a class's method names from Lua, only to look one up once you
// already know it, so register_external_type's type table auto-discovers methods
// lazily via a probe instance instead of requiring every one to be listed up front.
TEST(plugin, compiled_plugin_class_bridged_with_qualified_name_keeps_tracking)
{
    grunk::state grunk;

    grunk::PluginInfo info{"testplugin", "1.0"};
    sol::table ns = grunk.load_compiled_plugin(info, luaopen_testplugin);

    sol::table box_ctor = ns["Box"];
    grunk.register_external_type("testplugin.Box", box_ctor, ns);

    auto u = grunk.feature(4.);
    auto box = grunk.action("testplugin.Box.new", u);
    auto value = grunk.action("testplugin.Box.get_x", box);

    EXPECT_NEAR(value.value().as<double>(), 4., 1e-15);
    u.set_value(9.);
    EXPECT_NEAR(value.value().as<double>(), 9., 1e-15);

    // The serialized form must embed the fully-qualified path, since that's what has
    // to resolve when a saved recipe using this plugin is read back in later.
    auto serialized = value.compute_node()->serialize();
    EXPECT_NE(serialized.find("testplugin.Box.get_x"), std::string::npos);

    auto deserialized = grunk.deserialize(serialized);
    EXPECT_NEAR(deserialized.as<double>(), 9., 1e-15);
}

// The auto-discovery metamethod caches the bridged method as a plain function_meta
// entry on the type table, shared between original_env and decorated_env alike (see
// external_type_proxy). A plain, non-parametric environment must therefore be able to
// call an auto-discovered method directly and get a raw value back - not an action -
// exactly as it would for a method bridged via add_member_function.
TEST(plugin, compiled_plugin_class_auto_discovered_method_works_in_plain_environment)
{
    grunk::state grunk;

    grunk::PluginInfo info{"testplugin", "1.0"};
    sol::table ns = grunk.load_compiled_plugin(info, luaopen_testplugin);

    sol::table box_ctor = ns["Box"];
    grunk.register_external_type("testplugin.Box", box_ctor, ns);

    auto env = grunk.create_env();
    auto res = env.eval(R"(
        box = testplugin.Box.new(4.)
        x = testplugin.Box.get_x(box)
    )");
    ASSERT_TRUE(res.valid());

    auto x = env.get("x");
    EXPECT_NEAR(x.as<double>(), 4., 1e-15);
}
