// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>

namespace {

// A tiny stand-in for a plain C++ plugin's own type, registered directly against the
// namespace table begin_plugin returns - the shape geoml_plugin.cpp's register_geoml
// has today (see examples/cpp/cad_autodiff), just namespaced instead of flat.
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

TEST(PluginBegin, returns_empty_namespace_table_for_direct_registration)
{
    grunk::state grunk;

    sol::table ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});

    grunk.register_type<Length>("Length", ns, "cpp_plugin")
        .add_constructors([](double x) { return Length(x); })
        .add_member_function("set_x", &Length::set_x)
        .add_member_function("get_x", &Length::get_x);

    grunk.register_function(
        "twice",
        [](Length const& l) { return 2. * l.get_x(); },
        {},
        ns,
        "cpp_plugin"
    );

    // Registered under the plugin's own namespace, not flat in original_env.
    sol::table looked_up = grunk.get_type("cpp_plugin.Length");
    EXPECT_TRUE(looked_up.valid());
}

// A type/function registered via begin_plugin's namespace table must decorate exactly
// like a flat register_type call - dependency tracking, lazy evaluation, invalidation -
// mirroring dynamic_module.cpp's nested_type_keeps_dependency_tracking, which covers
// the same one-level-of-namespacing shape for run_module_script instead.
TEST(PluginBegin, registrations_keep_dependency_tracking)
{
    grunk::state grunk;

    sol::table ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});

    grunk.register_type<Length>("Length", ns, "cpp_plugin")
        .add_constructors([](double x) { return Length(x); })
        .add_member_function("get_x", &Length::get_x);

    grunk.register_function(
        "twice",
        [](Length const& l) { return 2. * l.get_x(); },
        {},
        ns,
        "cpp_plugin"
    );

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

// register_type/register_function's `qualifier` argument is what makes a nested
// registration serialize resolvably: without it, the constructor/function's own
// function_meta name would just be "Length.new"/"twice", which a saved recipe can't
// resolve back through cpp_plugin's namespace table on read-back.
TEST(PluginBegin, registrations_serialize_with_fully_qualified_name)
{
    grunk::state grunk;

    sol::table ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});
    grunk.register_type<Length>("Length", ns, "cpp_plugin")
        .add_constructors([](double x) { return Length(x); })
        .add_member_function("get_x", &Length::get_x);
    grunk.register_function(
        "twice",
        [](Length const& l) { return 2. * l.get_x(); },
        {},
        ns,
        "cpp_plugin"
    );

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

    sol::table ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.2.0"});
    grunk.register_function("one", []() { return 1; }, {}, ns, "cpp_plugin");

    ASSERT_EQ(grunk.plugins().size(), 1u);
    EXPECT_EQ(grunk.plugins()[0].version, "0.2.0");

    auto env = grunk.create_env();
    auto res = env.eval("s = cpp_plugin.one()");
    ASSERT_TRUE(res.valid());
    EXPECT_EQ(env.get("s").as<int>(), 1);
}

// register_type/register_function must auto-infer their `qualifier` argument from the
// begin_plugin table itself when the caller leaves it empty (see state::module_qualifier),
// so forgetting to pass info.name doesn't silently break a saved recipe's ability to
// resolve the constructor/function later (see registrations_serialize_with_fully_qualified_name
// above, which covers the same shape but with `qualifier` passed explicitly).
TEST(PluginBegin, qualifier_is_inferred_when_omitted)
{
    grunk::state grunk;

    sol::table ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});
    // Deliberately omit the trailing qualifier argument on both calls below.
    grunk.register_type<Length>("Length", ns)
        .add_constructors([](double x) { return Length(x); })
        .add_member_function("get_x", &Length::get_x);
    grunk.register_function(
        "twice",
        [](Length const& l) { return 2. * l.get_x(); },
        {},
        ns
    );

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

    sol::table ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});
    grunk.register_type<Length>("Length", ns, "cpp_plugin")
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

    sol::table ns = grunk.begin_plugin(grunk::PluginInfo{"cpp_plugin", "0.1.0"});
    grunk.register_function("one", []() { return 1; }, {}, ns, "cpp_plugin");

    // A plugin's entry point may call begin_plugin once and keep registering into the
    // returned table across several register_type/register_function calls - the table
    // itself doesn't need to be re-fetched or re-declared.
    grunk.register_function("two", []() { return 2; }, {}, ns, "cpp_plugin");

    auto env = grunk.create_env();
    auto res = env.eval("s = cpp_plugin.one() + cpp_plugin.two()");
    ASSERT_TRUE(res.valid());
    EXPECT_EQ(env.get("s").as<int>(), 3);
}
