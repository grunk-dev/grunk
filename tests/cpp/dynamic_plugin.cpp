// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>

namespace {

// A tiny stand-in for a compiled Lua C extension (e.g. a SWIG-Lua module): a plain
// C function matching the lua_CFunction/luaopen_* signature, built with sol2 for
// convenience but otherwise unaware of grunk. Mirrors the shape SWIG Lua modules
// have that actually matters for load_compiled_plugin/register_external_type:
// - a free function stored directly on the module table
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

// load_compiled_plugin used to silently reload the module table (luaL_requiref reuses
// an existing registry entry for the same name) while still appending a second entry to
// plugins() - it must now refuse a second load under an already-loaded name instead.
TEST(plugin, load_compiled_plugin_twice_with_same_name_throws)
{
    grunk::state grunk;

    grunk::PluginInfo info{"testplugin", "1.2.3"};
    grunk.load_compiled_plugin(info, luaopen_testplugin);
    EXPECT_THROW(
        grunk.load_compiled_plugin(grunk::PluginInfo{"testplugin", "9.9.9"}, luaopen_testplugin),
        grunk::io_error
    );

    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].version, "1.2.3");
}

namespace {

int g_reload_probe_open_count = 0;

int luaopen_reload_probe(lua_State* L)
{
    ++g_reload_probe_open_count;
    sol::state_view lua(L);
    sol::table mod = lua.create_table();
    mod["open_count"] = g_reload_probe_open_count;
    sol::stack::push(L, mod);
    return 1;
}

} // anonymous namespace

// clear_module's own doc comment calls it the sanctioned way to reload a plugin under
// the same name - that requires luaopen_reload_probe to actually run again on the
// second load_compiled_plugin call, not have luaL_requiref silently hand back the
// first call's cached module table via package.loaded.
TEST(plugin, clear_module_then_load_compiled_plugin_reinvokes_open_fn)
{
    grunk::state grunk;
    g_reload_probe_open_count = 0;

    grunk::PluginInfo info{"reload_probe", "1.0"};
    sol::table ns1 = grunk.load_compiled_plugin(info, luaopen_reload_probe);
    EXPECT_EQ(ns1.get<int>("open_count"), 1);

    grunk.clear_module("reload_probe");

    sol::table ns2 = grunk.load_compiled_plugin(grunk::PluginInfo{"reload_probe", "2.0"}, luaopen_reload_probe);
    EXPECT_EQ(ns2.get<int>("open_count"), 2);
}

// grunk::plugin::load_native's rollback path calls forget_plugin directly (not
// clear_module) when a plugin's grunk_plugin_register fails *after*
// load_compiled_plugin already succeeded (the shape examples/cpp/cad_autodiff's
// adtl_plugin.cpp has) - forget_plugin itself must therefore also clear the
// LUA_LOADED_TABLE cache entry luaL_requiref populates, exactly like clear_module does,
// otherwise a subsequent load attempt for the same name silently gets the stale cached
// module back instead of re-invoking open_fn.
TEST(plugin, forget_plugin_after_load_compiled_plugin_allows_reinvoking_open_fn)
{
    grunk::state grunk;
    g_reload_probe_open_count = 0;

    grunk::PluginInfo info{"reload_probe", "1.0"};
    sol::table ns1 = grunk.load_compiled_plugin(info, luaopen_reload_probe);
    EXPECT_EQ(ns1.get<int>("open_count"), 1);

    // Simulates load_native's own rollback for a plugin whose registration fails after
    // load_compiled_plugin already succeeded.
    grunk.forget_plugin("reload_probe");

    sol::table ns2 = grunk.load_compiled_plugin(grunk::PluginInfo{"reload_probe", "2.0"}, luaopen_reload_probe);
    EXPECT_EQ(ns2.get<int>("open_count"), 2);
}

// begin_plugin/load_compiled_plugin used to only check plugins() for a name collision,
// so a name already occupied by a plain run_module_script module would be silently
// clobbered instead of rejected - exactly the failure mode
// load_compiled_plugin_twice_with_same_name_throws above guards against for
// plugin-vs-plugin collisions, just for module-vs-plugin collisions instead.
TEST(plugin, load_compiled_plugin_rejects_name_already_used_by_module)
{
    grunk::state grunk;

    grunk.run_module_script("testplugin", "function helper() return 1 end");

    EXPECT_THROW(
        grunk.load_compiled_plugin(grunk::PluginInfo{"testplugin", "1.0"}, luaopen_testplugin),
        grunk::io_error
    );

    // The pre-existing module must be untouched by the rejected attempt.
    auto env = grunk.create_env();
    auto res = env.eval("y = testplugin.helper()");
    ASSERT_TRUE(res.valid());
    EXPECT_EQ(env.get("y").as<int>(), 1);
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

    // The serialized form must embed the fully-qualified path, since that's what has
    // to resolve when a saved recipe using this plugin is read back in later - a plain
    // "double_it(...)" would not resolve against the parametric environment, where the
    // function is only reachable as "testplugin.double_it".
    auto serialized = y.compute_node()->serialize();
    EXPECT_NE(serialized.find("testplugin.double_it"), std::string::npos);
}

// load_compiled_plugin's returned table used to lack the __grunk_module_name tag
// create_module stamps on begin_plugin's own namespace table, so register_function
// against it with the qualifier argument omitted couldn't auto-infer "testplugin" and
// would serialize an unresolvable, unqualified name instead.
TEST(plugin, load_compiled_plugin_namespace_supports_qualifier_auto_inference)
{
    grunk::state grunk;

    grunk::PluginInfo info{"testplugin", "1.0"};
    sol::table ns = grunk.load_compiled_plugin(info, luaopen_testplugin);

    // qualifier omitted - must auto-infer "testplugin" from ns itself, exactly like a
    // begin_plugin namespace table would.
    grunk.register_function("triple_it", [](double x) { return 3. * x; }, {}, ns);

    auto x = grunk.feature(2.);
    auto y = grunk.action("testplugin.triple_it", x);
    EXPECT_NEAR(y.value().as<double>(), 6., 1e-15);

    auto serialized = y.compute_node()->serialize();
    EXPECT_NE(serialized.find("testplugin.triple_it"), std::string::npos);
}

// This is the scenario create_decorated_environment's recursive decoration exists for:
// a class nested inside a plugin's own namespace table must still be tracked - both
// its constructor and its methods - exactly as a flat register_type registration would.
//
// Note there is no .add_member_function call for "get_x" here: SWIG-Lua bindings give
// no way to enumerate a class's method names from Lua, only to look one up once you
// already know it, so register_external_type's type table auto-discovers methods
// lazily via a probe instance instead of requiring every one to be listed up front.
//
// Also exercises plugin_namespace::register_external_type's auto-prefixing: "Box" (the
// class's own short name, not "testplugin.Box") is enough here - unlike the raw
// state::register_external_type API, which requires the caller to spell out the fully
// qualified name themselves (see register_external_type_throws_on_missing_namespace_prefix
// below).
TEST(plugin, compiled_plugin_class_bridged_with_qualified_name_keeps_tracking)
{
    grunk::state grunk;

    grunk::PluginInfo info{"testplugin", "1.0"};
    auto ns = grunk.load_compiled_plugin(info, luaopen_testplugin);

    sol::protected_function box_ctor = ns["Box"];
    ns.register_external_type("Box", box_ctor);

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

// register_external_type's `name` must already be fully qualified by the caller,
// unlike register_type/register_function which auto-infer their qualifier from a bare
// name - a caller forgetting the namespace prefix (passing "Box" instead of
// "testplugin.Box") used to silently register a type whose ctor/methods serialize with
// an unresolvable, unqualified name. It must now be rejected loudly instead.
TEST(plugin, register_external_type_throws_on_missing_namespace_prefix)
{
    grunk::state grunk;

    grunk::PluginInfo info{"testplugin", "1.0"};
    sol::table ns = grunk.load_compiled_plugin(info, luaopen_testplugin);

    sol::table box_ctor = ns["Box"];
    EXPECT_THROW(grunk.register_external_type("Box", box_ctor, ns), std::logic_error);
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
