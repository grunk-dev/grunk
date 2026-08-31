// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>

namespace {

// A tiny stand-in for a plain C++ plugin's own type, registered directly against the
// namespace begin_plugin returns - the shape geoml_plugin.cpp's register_geoml has
// today (see examples/cpp/cad_autodiff), just namespaced instead of flat.
struct Length
{
    Length() = default;
    Length(double x_) : x(x_) {}
    void set_x(double x_) { x = x_; }
    double get_x() const { return x; }
    double x{0.};
};

} // anonymous namespace

TEST(PluginBegin, records_metadata_immediately)
{
    grunk::state grunk;

    grunk::PluginInfo info{"cpp_plugin", "0.1.0"};
    grunk.begin_plugin(info);

    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].name, "cpp_plugin");
    EXPECT_EQ(grunk.plugins()[0].version, "0.1.0");
}

TEST(PluginBegin, returns_empty_namespace_for_direct_registration)
{
    grunk::state grunk;

    auto ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});

    ns.register_type<Length>("Length")
        .add_constructors([](double x) { return Length(x); })
        .add_member_function("set_x", &Length::set_x)
        .add_member_function("get_x", &Length::get_x);

    ns.register_function("twice", [](Length const& l) { return 2. * l.get_x(); });

    // Registered under the plugin's own namespace, not flat in original_env.
    sol::table looked_up = grunk.get_type("cpp_plugin.Length");
    EXPECT_TRUE(looked_up.valid());
}

// A type/function registered via begin_plugin's namespace must decorate exactly like a
// flat register_type call - dependency tracking, lazy evaluation, invalidation -
// mirroring dynamic_module.cpp's nested_type_keeps_dependency_tracking, which covers
// the same one-level-of-namespacing shape for run_module_script instead.
TEST(PluginBegin, registrations_keep_dependency_tracking)
{
    grunk::state grunk;

    auto ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});

    ns.register_type<Length>("Length")
        .add_constructors([](double x) { return Length(x); })
        .add_member_function("get_x", &Length::get_x);

    ns.register_function("twice", [](Length const& l) { return 2. * l.get_x(); });

    auto penv = grunk.create_parametric_env();
    auto res = penv.eval(R"(
        u = grunk.feature(3.)
        l = cpp_plugin.Length.new(u)
        x = cpp_plugin.twice(l)
    )");
    ASSERT_TRUE(res.valid());

    auto fx = penv.get_feature("x");
    EXPECT_NEAR(fx.value().as<double>(), 6.0, 1e-15);

    auto fu = penv.get_feature("u");
    fu.set_value(5.0);
    EXPECT_NEAR(fx.value().as<double>(), 10.0, 1e-15);
}

// plugin_namespace::register_type/register_function auto-supply the namespace table and
// qualifier (see state::begin_plugin/plugin_namespace's own doc comments) - without
// that, the constructor/function's own function_meta name would just be
// "Length.new"/"twice", which a saved recipe can't resolve back through cpp_plugin's
// namespace table on read-back.
TEST(PluginBegin, registrations_serialize_with_fully_qualified_name)
{
    grunk::state grunk;

    auto ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});
    ns.register_type<Length>("Length")
        .add_constructors([](double x) { return Length(x); })
        .add_member_function("get_x", &Length::get_x);
    ns.register_function("twice", [](Length const& l) { return 2. * l.get_x(); });

    auto u = grunk.feature(3.);
    auto l = grunk.action("cpp_plugin.Length.new", u);
    auto x = grunk.action("cpp_plugin.twice", l);

    auto serialized_ctor = l.compute_node()->serialize();
    EXPECT_NE(serialized_ctor.find("cpp_plugin.Length.new"), std::string::npos);

    auto serialized_fun = x.compute_node()->serialize();
    EXPECT_NE(serialized_fun.find("cpp_plugin.twice"), std::string::npos);

    auto deserialized = grunk.deserialize(serialized_fun);
    EXPECT_NEAR(deserialized.as<double>(), 6.0, 1e-15);
}

// begin_plugin used to silently overwrite an existing plugin's namespace table if
// called twice with the same name (state::create_module always minted a fresh table) -
// it must now refuse instead, so a double-load (or a name collision between two
// distinct plugins) fails loudly rather than making the first plugin's types/functions
// unreachable from Lua while plugins() still lists them.
TEST(PluginBegin, calling_twice_with_same_name_throws)
{
    grunk::state grunk;

    grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});
    EXPECT_THROW(grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.2.0"}), grunk::io_error);

    // The first plugin's identity (not the rejected second one) must still be the only
    // one recorded.
    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].version, "0.1.0");
}

// clear_module is the sanctioned way to force a reload under the same name (see its own
// doc comment) - it must also forget the plugin's identity, not just its Lua table, so a
// subsequent begin_plugin call under the same name is accepted rather than rejected by
// the same guard calling_twice_with_same_name_throws exercises above.
TEST(PluginBegin, clear_module_allows_reload_under_same_name)
{
    grunk::state grunk;

    grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});
    grunk.clear_module("cpp_plugin");

    EXPECT_EQ(grunk.plugins().size(), 0u);

    auto ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.2.0"});
    ns.register_function("one", []() { return 1; });

    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].version, "0.2.0");

    auto env = grunk.create_env();
    auto res = env.eval("s = cpp_plugin.one()");
    ASSERT_TRUE(res.valid());
    EXPECT_EQ(env.get("s").as<int>(), 1);
}

// plugin_namespace is implicitly convertible to sol::table (and .table()/[] expose the
// wrapped table directly), an escape hatch for code that still needs the raw
// state::register_type/register_function API - e.g. to override the namespace's own
// qualifier with something else entirely, which plugin_namespace's own register_type/
// register_function don't allow (they always use the namespace's own qualifier).
TEST(PluginBegin, namespace_table_interoperates_with_raw_state_api)
{
    grunk::state grunk;

    auto ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});

    // Passing ns.table() (or ns itself, via the implicit conversion) to the raw
    // state::register_type/register_function API still works, and an explicit
    // qualifier here still overrides ns's own auto-inferred one.
    grunk.register_type<Length>("Length", ns.table(), "overridden")
        .add_constructors([](double x) { return Length(x); })
        .add_member_function("get_x", &Length::get_x);
    grunk.register_function(
        "twice",
        [](Length const& l) { return 2. * l.get_x(); },
        {}, ns, "overridden"
    );

    // Still reachable through the namespace table itself (the qualifier only affects
    // the *serialized* name, not where the symbols actually live).
    auto env = grunk.create_env();
    auto res = env.eval("x = cpp_plugin.twice(cpp_plugin.Length.new(3.))");
    ASSERT_TRUE(res.valid());
    EXPECT_NEAR(env.get<double>("x"), 6., 1e-15);

    auto u = grunk.feature(3.);
    auto l = grunk.action("cpp_plugin.Length.new", u);
    auto serialized = l.compute_node()->serialize();
    EXPECT_NE(serialized.find("overridden.Length.new"), std::string::npos);
}

// forget_plugin used to only remove a plugin's Lua-visible namespace, leaving its
// registered C++ types reachable via the process-wide type registry (m_type_registry/
// m_type_names, see register_type) - directly contradicting forget_plugin's own doc
// comment, which promises partially-registered types/functions aren't left reachable
// either. A DynamicFeature carrying a type hint for the forgotten type (constructed
// before the rollback, so its type hint comes from construction, never from evaluating
// it) must no longer be able to dispatch methods against that type afterwards.
TEST(PluginBegin, forget_plugin_removes_type_registry_entries)
{
    grunk::state grunk;

    auto ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});
    ns.register_type<Length>("Length")
        .add_constructors([](double x) { return Length(x); })
        .add_member_function("get_x", &Length::get_x);

    auto l = grunk.feature("cpp_plugin.Length", 4.0);

    grunk.forget_plugin("cpp_plugin");

    // Before the fix, m_type_registry/m_type_names still held Length's usertype table,
    // so this colon-call dispatch would still succeed even though the plugin was fully
    // rolled back.
    EXPECT_THROW(l.call("get_x"), std::runtime_error);
}

TEST(PluginBegin, incremental_registration_is_allowed)
{
    grunk::state grunk;

    auto ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});
    ns.register_function("one", []() { return 1; });

    // A plugin's entry point may call begin_plugin once and keep registering against
    // the returned namespace across several register_type/register_function calls - it
    // doesn't need to be re-fetched or re-declared.
    ns.register_function("two", []() { return 2; });

    auto env = grunk.create_env();
    auto res = env.eval("s = cpp_plugin.one() + cpp_plugin.two()");
    ASSERT_TRUE(res.valid());
    EXPECT_EQ(env.get("s").as<int>(), 3);
}
