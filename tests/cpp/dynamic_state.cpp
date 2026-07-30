// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
#include <cmath>
#include <tuple>
#include <typeindex>

namespace {

    double add(double l, double r) {
        return l + r;
    }

} // anonymous namespace

TEST(state, simple_parametric)
{
    grunk::state grunk;

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = grunk.feature(2.)
        y = grunk.feature(38.)

        a = x ^ 2
        b = a + y  

        b1 = b:value()

        x:set_value(1)

        b2 = b:value()
    )");

    auto b1 = env.get("b1").as<double>();
    EXPECT_NEAR(b1, 42, 1e-15);

    auto b2 = env.get("b2").as<double>();
    EXPECT_NEAR(b2, 39, 1e-15);

    auto y = env.get_feature("y"); // short for env.get<grunk::DynamicFeature>("y")
    y.set_value(41);

    auto b3 = env.get_feature("b").value().as<double>();
    EXPECT_NEAR(b3, 42, 1e-15);
}

TEST(state, free_function_registration)
{
    grunk::state grunk;
    grunk.register_function("add", &add);
    auto f = grunk.get_function("add"); // just to see that it exists
    auto env = grunk.create_env();
    env.eval(
        R"(
        x = 17
        y = 25
        z = add(x,y)
        )"
    );
    double z = env.get<double>("z");
    ASSERT_NEAR(z, 42, 1e-14);
}

TEST(state, object)
{
    grunk::state grunk;
    auto x = grunk.create_object(2.);
    auto y = grunk.create_object(3.);
    auto z = x + y;
    ASSERT_NEAR(z.as<double>(), 5., 1e-14);
}

TEST(state, environment_get)
{
    grunk::state grunk;
    auto env = grunk.create_env();
    env.eval("x = 17");
    double x = env.get<double>("x");
    ASSERT_NEAR(x, 17, 1e-14);
    ASSERT_THROW(env.get<double>("y"), std::runtime_error);
}

TEST(state, free_function_cpp)
{
    grunk::state grunk;

    grunk.register_function("add", &add);

    auto x = grunk.feature(2.);
    auto y = grunk.feature(1.);
    auto z = grunk.action("add", x, y);

    EXPECT_FALSE(z.is_valid());
    double zv = z.value().as<double>();
    EXPECT_TRUE(z.is_valid());
    ASSERT_NEAR(zv, 3., 1e-14);

    x.set_value(5.);

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(x.value().as<double>(), 5., 1e-14);
    zv = z.value().as<double>();
    EXPECT_TRUE(z.is_valid());
    ASSERT_NEAR(zv, 6., 1e-14);

}

TEST(state, free_function_cpp_mixed_static_input_to_dynamic_action)
{
    grunk::state grunk;

    grunk.register_function("add", &add);

    auto x = grunk::feature(2.); // static feature
    auto y = grunk.feature(1.); // dynamic feature
    auto z = grunk.action("add", x, y);

    EXPECT_FALSE(z.is_valid());
    double zv = z.value().as<double>();
    EXPECT_TRUE(z.is_valid());
    ASSERT_NEAR(zv, 3., 1e-14);

    x.set_value(5.);

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(x.value(), 5., 1e-14);
    zv = z.value().as<double>();
    EXPECT_TRUE(z.is_valid());
    ASSERT_NEAR(zv, 6., 1e-14);

}

TEST(state, free_function_lua)
{
    grunk::state grunk;

    grunk.register_function("add", &add);

    auto env = grunk.create_parametric_env();
    env.eval(R"(
       x = grunk.feature(2.)
       y = grunk.feature(1.)
       z = add(x,y)
    )");


    auto x = env.get_feature("x");
    auto y = env.get_feature("y");
    auto z = env.get_feature("z");

    EXPECT_FALSE(z.is_valid());
    double zv = z.value().as<double>();
    EXPECT_TRUE(z.is_valid());
    ASSERT_NEAR(zv, 3., 1e-14);

    x.set_value(5.);

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(env.get_feature("x").value().as<double>(), 5., 1e-14);
    EXPECT_NEAR(x.value().as<double>(), 5., 1e-14);
    zv = z.value().as<double>();
    EXPECT_TRUE(z.is_valid());
    ASSERT_NEAR(zv, 6., 1e-14);
}

TEST(state, free_function_feature_id_lua)
{
    grunk::state grunk;

    grunk.register_function("add", &add);

    auto env = grunk.create_parametric_env();
    env.eval(R"(

       x = grunk.feature(2.):with_id('x')
       y = grunk.feature(1.):with_id('y')
       local z = add(x,y):with_id('z')

       z_id1 = z:id()
       z:set_id('horst')
       z_id2 = z:id()

       z_value1 = z:value()

       x:set_value(42.)

       z_value2 = z:value()

    )");

    EXPECT_EQ(env.get_feature("x").id(), "x");
    EXPECT_EQ(env.get_feature("y").id(), "y");
    EXPECT_EQ(env.get<std::string>("z_id1"), "z");
    EXPECT_EQ(env.get<std::string>("z_id2"), "horst");

    EXPECT_NEAR(env.get<double>("z_value1"), 3., 1e-14);
    EXPECT_NEAR(env.get<double>("z_value2"), 43., 1e-14);

}


namespace {

class MyScalar
{
public:
    MyScalar(double v) : m_value(v) {}
    double value() const
    {
        return m_value;
    }

    MyScalar pow(double exponent) {
        return ::pow(m_value, exponent);
    }

    void set(double v) {
        m_value = v;
    }
private:
    double m_value;
};

MyScalar operator+(MyScalar const& l, MyScalar const& r) {
    return MyScalar(l.value() + r.value());
}

struct DefaultConstructible {};

} // anonymous namespace


TEST(state, usertype_ctor_cpp)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    );

    grunk.register_type<DefaultConstructible>("DefaultConstructible");

    {
        // string overloads
        auto a = grunk.feature(1.);
        auto x = grunk.action("MyScalar:new", a);       // ctor as action: x depends on a
        auto y = grunk.action("MyScalar.new", 2.);      // ctor as action with argument conversion from contant: y depends on Feature(2.)
        auto z = grunk.feature("MyScalar", 3.);          // forwards ctor args to grunk::feature: z is independent feature

        EXPECT_EQ(x.node_pointer()->get_parents().size(), 1);
        EXPECT_EQ(y.node_pointer()->get_parents().size(), 1);
        EXPECT_EQ(z.node_pointer()->get_parents().size(), 0);

        EXPECT_EQ(x.value().as<MyScalar>().value(), 1.);
        EXPECT_EQ(y.value().as<MyScalar>().value(), 2.);
        EXPECT_EQ(z.value().as<MyScalar>().value(), 3.);

        a.set_value(4.);
        EXPECT_EQ(x.value().as<MyScalar>().value(), 4.);
    }

    {
        // sol::function/sol::table overloads

        // gets the original undecorated type and constructor function
        sol::table MyScalarT = grunk.get_type("MyScalar");
        auto MyScalarCtor = grunk.get_function("MyScalar.new");

        auto a = grunk.feature(1.);
        auto x = grunk.action(MyScalarCtor, a);       // ctor as action: x depends on a
        auto y = grunk.action(MyScalarCtor, 2.);      // ctor as action with argument conversion from contant: y depends on Feature(2.)
        auto z = grunk.feature(MyScalarT, 3.);        // forwards ctor args to grunk::feature: z is independent feature

        EXPECT_EQ(x.node_pointer()->get_parents().size(), 1);
        EXPECT_EQ(y.node_pointer()->get_parents().size(), 1);
        EXPECT_EQ(z.node_pointer()->get_parents().size(), 0);

        EXPECT_EQ(x.value().as<MyScalar>().value(), 1.);
        EXPECT_EQ(y.value().as<MyScalar>().value(), 2.);
        EXPECT_EQ(z.value().as<MyScalar>().value(), 3.);

        a.set_value(4.);
        EXPECT_EQ(x.value().as<MyScalar>().value(), 4.);
    }


    {
        // make sure grunk.feature can accept both strings as values, as well as default constructible types represented by string

        // this is a Feature containing a string
        auto z1 = grunk.feature("DefaultConstructible");
        EXPECT_TRUE(z1.value().is<std::string>());

        // method 1: this is a Feature containing a DefaultConstructible instance
        sol::table DefaultConstructibleT = grunk.get_type("DefaultConstructible");
        auto z2 = grunk.feature(DefaultConstructibleT);
        EXPECT_TRUE(z2.value().is<DefaultConstructible>());

        // method 3: this is also a feature containing a DefaultConstructile instance
        auto z3 = grunk.feature("DefaultConstructible", grunk::default_construct);
        EXPECT_TRUE(z2.value().is<DefaultConstructible>());
    }

}


TEST(state, usertype_ctor_as_action_lua)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    );

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        local a = grunk.feature(1.)

        x = MyScalar.new(a)             -- ctor as action: x depends on a
        y = MyScalar.new(2.)            -- ctor as action with argument conversion from constant: y depends on Feature(2.)
        z = MyScalar.new_feature(3.)    -- forwards ctor args to grunk::feature: z is independent feature

        x1 = x:value()
        a:set_value(4.)
        x2 = x:value()
    )");

    EXPECT_EQ(env.get_feature("x").node_pointer()->get_parents().size(), 1);
    EXPECT_EQ(env.get_feature("y").node_pointer()->get_parents().size(), 1);
    EXPECT_EQ(env.get_feature("z").node_pointer()->get_parents().size(), 0);

    EXPECT_EQ(env.get_feature("y").value().as<MyScalar>().value(), 2.);
    EXPECT_EQ(env.get_feature("z").value().as<MyScalar>().value(), 3.);

    EXPECT_EQ(env.get<MyScalar>("x1").value(), 1.);
    EXPECT_EQ(env.get<MyScalar>("x2").value(), 4.);
    EXPECT_EQ(env.get_feature("x").value().as<MyScalar>().value(), 4.);
}

TEST(state, usertype_operators_as_action_lua)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("__add", [](MyScalar const& l, MyScalar const&r){
            return l + r;
    })
    .add_member_function("set", &MyScalar::set)
    .add_data_member("value", &MyScalar::value);

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        local x = MyScalar.new_feature(2.)
        local y = MyScalar.new_feature(3.)
        local z = x + y

        z1 = z:value()

        x:change_value():set(39.)

        z2 = z:value()
    )");

    EXPECT_NEAR(env.get<MyScalar>("z1").value(), 5., 1e-14);
    EXPECT_NEAR(env.get<MyScalar>("z2").value(), 42., 1e-14);
}

TEST(state, usertype_method_as_action_cpp)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("set", &MyScalar::set)
    .add_member_function("pow", &MyScalar::pow);

    auto x = grunk.action("MyScalar.new", 2.);

    // test the "method as free function" syntax
    {
        // colon-syntax
        auto y = grunk.action("MyScalar:pow", x, 3);
        EXPECT_NEAR(y.value().as<MyScalar>().value(), 8, 1e-14);
        x.set_value(MyScalar(3));
        EXPECT_NEAR(y.value().as<MyScalar>().value(), 27, 1e-14);
    }
    {
        // dot-syntax
        auto y = grunk.action("MyScalar.pow", x, 3);
        EXPECT_NEAR(y.value().as<MyScalar>().value(), 27, 1e-14);
        x.set_value(MyScalar(2));
        EXPECT_NEAR(y.value().as<MyScalar>().value(), 8, 1e-14);
    }

    // test the Feature::as syntax
    grunk::object z = x.as("MyScalar")["pow"](3);
    ASSERT_TRUE(z.valid());
    ASSERT_TRUE(z.is<grunk::DynamicFeature>());
    auto zf = z.as<grunk::DynamicFeature>();
    EXPECT_NEAR(zf.value().as<MyScalar>().value(), 8, 1e-14);
    x.set_value(MyScalar(3));
    EXPECT_NEAR(zf.value().as<MyScalar>().value(), 27, 1e-14);

}

TEST(state, usertype_method_as_action_lua)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("set", &MyScalar::set)
    .add_member_function("pow", &MyScalar::pow);

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        local x = MyScalar.new_feature(2)

        -- test the "method as free function" syntax

        local y = MyScalar.pow(x, 2)

       y1 = y:value()
       x:change_value():set(3)
       y2 = y:value()

        -- test the Feature:as syntax

        local z = x:as(MyScalar).pow(3)

        z1 = z:value()
        x:change_value():set(2)
        z2 = z:value()
    )");

    EXPECT_NEAR(env.get<MyScalar>("y1").value(), 4, 1e-14); // 2^2
    EXPECT_NEAR(env.get<MyScalar>("y2").value(), 9, 1e-14); // 3^2

    EXPECT_NEAR(env.get<MyScalar>("z1").value(),27, 1e-14); // 3^3
    EXPECT_NEAR(env.get<MyScalar>("z2").value(), 8, 1e-14); // 2^3
}

// usertype_method_as_action_lua above only ever calls :as() on a "root" feature
// (MyScalar.new_feature(2), constructed directly from an object - see DynamicFeature's
// Feature(object const&) ctor, which knows its lua state up front). A feature that is
// itself the result of a computation (like y below, from MyScalar.pow(x, 2)) is instead
// constructed from a parametric::param, which used not to know its lua state at all
// (DynamicFeature.hpp's other ctor) - so :as() on anything computed, not just literal
// features, threw "DynamicFeature: lua state is uninitialized". That failure used to be
// swallowed silently rather than surfaced: function_meta::operator() returned the failed
// protected_function_result as-is, so the exception's message came back out as if it were
// a normal (string) return value instead of failing the eval - hence asserting
// result.valid() here, not just the computed values.
TEST(state, usertype_method_as_action_on_computed_feature_lua)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("set", &MyScalar::set)
    .add_member_function("pow", &MyScalar::pow);

    auto env = grunk.create_parametric_env();
    auto result = env.eval(R"(
        x = MyScalar.new_feature(2)
        y = MyScalar.pow(x, 2) -- y is computed, not a "new_feature" root

        z = y:as(MyScalar).pow(3)
        z1 = z:value()
        x:change_value():set(3)
        z2 = z:value()
    )");
    ASSERT_TRUE(result.valid());

    EXPECT_NEAR(env.get<MyScalar>("z1").value(), 64, 1e-14);  // (2^2)^3 = 64
    EXPECT_NEAR(env.get<MyScalar>("z2").value(), 729, 1e-14); // (3^2)^3 = 729
}

TEST(state, placeholder_feature_cpp)
{
    grunk::state grunk;
    auto x = grunk.feature().with_id("x");
    auto y = grunk.feature().with_id("y");
    auto z = x + y;
    z.set_id("z");
    EXPECT_TRUE(x.is_placeholder());
    EXPECT_TRUE(y.is_placeholder());
    EXPECT_THROW(z.value(), std::runtime_error);
    x.set_value(1);
    y.set_value(2);
    EXPECT_FALSE(x.is_placeholder());
    EXPECT_FALSE(y.is_placeholder());
    EXPECT_NEAR(z.value().as<double>(), 3., 1e-14);
}

TEST(state, placeholder_feature_lua)
{
    grunk::state grunk;

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = grunk.feature()
        y = grunk.feature()
        z = x + y
    )");
    env.tag_features();

    auto x = env.get_feature("x");
    auto y = env.get_feature("y");
    auto z = env.get_feature("z");

    EXPECT_TRUE(x.is_placeholder());
    EXPECT_TRUE(y.is_placeholder());
    EXPECT_THROW(z.value(), std::runtime_error);

    x.set_value(1);
    y.set_value(2);
    EXPECT_FALSE(x.is_placeholder());
    EXPECT_FALSE(y.is_placeholder());
    EXPECT_NEAR(z.value().as<double>(), 3., 1e-14);
}

// --- native colon-call dispatch on DynamicFeature (grunk issue #269) ---
//
// These tests cover DynamicFeature's __index-based method dispatch (see the
// sol::meta_function::index handler registered on the Feature usertype in
// state::init(), state.hpp), which lets plain Lua colon-call syntax
// (`feature:method(args)`) reach a registered C++ type's methods directly, without
// DynamicFeature::as() or the qualified TypeName.method(instance, args) form. It works
// by consulting a type hint stashed on the feature at construction time (see
// DynamicFeature::set_type_hint/type_hint) - never by evaluating the feature's value,
// which would defeat lazy evaluation/caching (see test native_colon_call_no_eager_evaluation
// below, which asserts on this directly).

TEST(state, native_colon_call_root_feature)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", &MyScalar::pow);

    // grunk.feature(T const&) is templated on T, so it can stamp a type hint right
    // away - see state::feature<T>.
    auto x = grunk.feature(MyScalar(2.));

    auto env = grunk.create_parametric_env();
    env["x"] = x;
    env.eval("result = x:pow(3)");

    auto result = env.get_feature("result");
    EXPECT_EQ(result.node_pointer()->get_parents().size(), 1);
    EXPECT_NEAR(result.value().as<MyScalar>().value(), 8., 1e-14); // 2^3
}

TEST(state, native_colon_call_right_after_construction)
{
    // A constructor's result is always exactly T, so usertype_proxy::add_constructors
    // stamps the type hint directly (it can't be deduced: every constructor call is
    // wrapped in sol::overload(...), even a single one, and sol::overload_set isn't
    // introspectable via function_traits). This is what lets colon-call work
    // immediately on a freshly-constructed feature, e.g. right after
    // MyScalar.new_feature(...) or MyScalar.new(...), with no intervening
    // member-function action needed to pick up a hint.
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", &MyScalar::pow);

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        local x = MyScalar.new_feature(2)
        result = x:pow(3)
    )");

    EXPECT_NEAR(env.get_feature("result").value().as<MyScalar>().value(), 8., 1e-14); // 2^3
}

TEST(state, native_colon_call_no_eager_evaluation)
{
    grunk::state grunk;

    int pow_call_count = 0;
    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", [&pow_call_count](MyScalar& self, double exponent) {
        ++pow_call_count;
        return self.pow(exponent);
    });

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        local x = MyScalar.new_feature(2)
        y = MyScalar.pow(x, 2)  -- y: computed feature, tagged with a MyScalar type hint
                                 -- by ActionDynamic::initialize_results, before ever running
        z = y:pow(3)             -- resolved via native colon-call dispatch - must NOT
                                  -- force y (or z) to be evaluated just to look up "pow"
    )");

    EXPECT_EQ(pow_call_count, 0);

    EXPECT_NEAR(env.get_feature("z").value().as<MyScalar>().value(), 64., 1e-10); // (2^2)^3
    EXPECT_EQ(pow_call_count, 2); // now both y's and z's pow() have actually run
}

TEST(state, native_colon_call_argument_order)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("combine", [](MyScalar const& self, double a, double b) {
        // encodes self/a/b positionally so a self/argument shift (the :as() double-self
        // bug this dispatch mechanism avoids) would be caught by the expected value below
        return self.value() * 100. + a * 10. + b;
    });

    auto x = grunk.feature(MyScalar(2.));
    auto env = grunk.create_parametric_env();
    env["x"] = x;
    env.eval("result = x:combine(3, 4)");

    EXPECT_NEAR(env.get_feature("result").value().as<double>(), 234., 1e-10); // 2*100 + 3*10 + 4
}

TEST(state, native_colon_call_no_hint_throws)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", &MyScalar::pow);

    auto env = grunk.create_parametric_env();
    // grunk.feature(42) goes through the untemplated grunk::object overload - no C++
    // type was ever in hand at that call site, so no hint is available.
    env.eval("x = grunk.feature(42)");

    bool threw = false;
    try {
        env.eval("z = x:pow(3)");
    } catch (std::exception const& e) {
        threw = true;
        EXPECT_NE(std::string(e.what()).find("no static type information"), std::string::npos);
    }
    EXPECT_TRUE(threw);
}

TEST(state, native_colon_call_unregistered_type_hint_throws)
{
    // grunk.feature<T>(value) (the templated state::feature overload) always stamps a
    // type hint from typeid(T), regardless of whether T was ever registered as a
    // usertype via register_type - e.g. a plain double never is. This is a distinct
    // failure mode from "no hint at all" (see native_colon_call_no_hint_throws above)
    // and gets its own error message (see the __index handler in state.hpp's init()).
    grunk::state grunk;

    auto x = grunk.feature(4.2);
    auto env = grunk.create_parametric_env();
    env["x"] = x;

    bool threw = false;
    try {
        env.eval("z = x:foo()");
    } catch (std::exception const& e) {
        threw = true;
        EXPECT_NE(std::string(e.what()).find("no corresponding usertype registered"), std::string::npos);
    }
    EXPECT_TRUE(threw);
}

TEST(state, native_colon_call_unknown_member_on_known_type_throws)
{
    // A type hint pointing at a properly-registered type, but a method name that isn't
    // one of its members, is a third distinct failure mode (most likely a typo'd method
    // name) and should not be confused with "no static type information available".
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", &MyScalar::pow);

    auto x = grunk.feature(MyScalar(2.));
    auto env = grunk.create_parametric_env();
    env["x"] = x;

    bool threw = false;
    try {
        env.eval("z = x:not_a_real_method()");
    } catch (std::exception const& e) {
        threw = true;
        std::string const msg = e.what();
        EXPECT_NE(msg.find("MyScalar"), std::string::npos);
        EXPECT_NE(msg.find("has no member"), std::string::npos);
    }
    EXPECT_TRUE(threw);
}

TEST(state, native_colon_call_reserved_name_shadowing)
{
    grunk::state grunk;

    // MyScalar has its own "value" member, colliding with Feature's built-in value().
    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("value", &MyScalar::value);

    auto x = grunk.feature(MyScalar(2.));
    auto env = grunk.create_parametric_env();
    env["x"] = x;
    env.eval("result = x:value()");

    // Feature's own value() always wins: sol2 resolves Feature's directly-registered
    // members before the __index fallback ever runs, so this returns the wrapped
    // MyScalar itself (Feature::value()'s result), not a new action computing
    // MyScalar::value() (which would yield a plain double).
    grunk::object result = env.get("result");
    ASSERT_TRUE(result.is<MyScalar>());
    EXPECT_NEAR(result.as<MyScalar>().value(), 2., 1e-14);
}

TEST(state, native_colon_call_void_return_no_hint)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("set", &MyScalar::set); // void-returning

    auto x = grunk.feature(MyScalar(2.));
    auto env = grunk.create_parametric_env();
    env["x"] = x;
    env.eval("y = x:set(5.)");

    grunk::DynamicFeature y = env.get_feature("y");
    EXPECT_FALSE(y.type_hint().has_value());
    EXPECT_NO_THROW(y.value());
}

TEST(state, deduce_return_type_hint_tuple_return_is_safe)
{
    // grunk::details::deduce_return_type_hint is the compile-time hook that populates
    // function_meta::return_type_hint (see function_meta.hpp). A multi-output
    // (std::tuple-returning) callable must not crash this deduction - it just gets no
    // hint, same as the void-return case above.
    auto hint = grunk::details::deduce_return_type_hint<std::tuple<double,double>(*)(MyScalar const&)>();
    EXPECT_FALSE(hint.has_value());
}

TEST(state, native_colon_call_clone_propagates_type_hint)
{
    grunk::state grunk;

    int pow_call_count = 0;
    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", [&pow_call_count](MyScalar& self, double exponent) {
        ++pow_call_count;
        return self.pow(exponent);
    });

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        local x = MyScalar.new_feature(2)
        y = MyScalar.pow(x, 2)
    )");

    grunk::DynamicFeature y = env.get_feature("y");
    ASSERT_TRUE(y.type_hint().has_value());

    grunk::DynamicFeature y_clone = y.clone();
    ASSERT_TRUE(y_clone.type_hint().has_value());
    EXPECT_EQ(*y_clone.type_hint(), *y.type_hint());

    // colon-call dispatch on the clone must resolve without ever evaluating the
    // original or the clone - this is exactly why DynamicFeature::clone() needed to
    // carry the type hint across (see DynamicFeature::clone()).
    env["y_clone"] = y_clone;
    env.eval("z = y_clone:pow(3)");

    EXPECT_EQ(pow_call_count, 0);

    EXPECT_NEAR(env.get_feature("z").value().as<MyScalar>().value(), 64., 1e-10); // (2^2)^3
    EXPECT_EQ(pow_call_count, 2);
}

namespace {

// A type that is never registered via grunk::state::register_type in the test below -
// only via modify_type, on a usertype table built directly through sol2. This stands in
// for modify_type's documented use case (a usertype that arrived some other way, e.g. a
// plugin), so the test genuinely exercises modify_type's own type-registry population
// rather than piggy-backing on a register_type call for the same C++ type.
class ModifyTypeScalar
{
public:
    ModifyTypeScalar(double v) : m_value(v) {}
    double doubled() const { return m_value * 2.; }
private:
    double m_value;
};

} // anonymous namespace

TEST(state, modify_type_only_registration_native_colon_call)
{
    grunk::state grunk;

    // Grab this state's actual lua_State via a throwaway feature, so the usertype we
    // build below lives in the same Lua state grunk::state itself uses internally.
    auto probe = grunk.feature(1);
    sol::state_view lua(probe.lua_state());

    sol::table plugin_ns = lua.create_table();
    plugin_ns.new_usertype<ModifyTypeScalar>("ModifyTypeScalar");

    // register_type is never called for ModifyTypeScalar - modify_type is the only
    // thing that should make native colon-call dispatch work for it.
    grunk.modify_type<ModifyTypeScalar>("ModifyTypeScalar", plugin_ns)
        .add_member_function("doubled", &ModifyTypeScalar::doubled);

    auto x = grunk.feature(ModifyTypeScalar(21.));
    auto env = grunk.create_parametric_env();
    env["x"] = x;
    env.eval("z = x:doubled()");

    EXPECT_NEAR(env.get_feature("z").value().as<double>(), 42., 1e-14);
}

// --- DynamicFeature::call() - the C++-side equivalent of Lua colon-call ---

TEST(state, call_relative_name)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", &MyScalar::pow);

    auto x = grunk.feature(MyScalar(2.));

    grunk::object result = x.call("pow", 3);
    ASSERT_TRUE(result.is<grunk::DynamicFeature>());
    auto result_feature = result.as<grunk::DynamicFeature>();
    EXPECT_EQ(result_feature.node_pointer()->get_parents().size(), 1);
    EXPECT_NEAR(result_feature.value().as<MyScalar>().value(), 8., 1e-14); // 2^3
}

TEST(state, call_fully_qualified_name)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", &MyScalar::pow);

    // grunk.feature(T const&) does stamp a type hint, but a fully-qualified name must
    // work regardless - it never consults the hint at all.
    auto x = grunk.feature(MyScalar(2.));

    grunk::object dot_result = x.call("MyScalar.pow", 3);
    grunk::object colon_result = x.call("MyScalar:pow", 3); // separator is cosmetic

    EXPECT_NEAR(dot_result.as<grunk::DynamicFeature>().value().as<MyScalar>().value(), 8., 1e-14);
    EXPECT_NEAR(colon_result.as<grunk::DynamicFeature>().value().as<MyScalar>().value(), 8., 1e-14);
}

TEST(state, call_no_eager_evaluation)
{
    grunk::state grunk;

    int pow_call_count = 0;
    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", [&pow_call_count](MyScalar& self, double exponent) {
        ++pow_call_count;
        return self.pow(exponent);
    });

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        local x = MyScalar.new_feature(2)
        y = MyScalar.pow(x, 2)
    )");

    grunk::DynamicFeature y = env.get_feature("y");
    grunk::object z = y.call("pow", 3); // must not evaluate y just to resolve "pow"

    EXPECT_EQ(pow_call_count, 0);

    EXPECT_NEAR(z.as<grunk::DynamicFeature>().value().as<MyScalar>().value(), 64., 1e-10); // (2^2)^3
    EXPECT_EQ(pow_call_count, 2);
}

TEST(state, call_no_hint_relative_name_throws)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("pow", &MyScalar::pow);

    auto env = grunk.create_parametric_env();
    env.eval("x = grunk.feature(42)"); // untemplated overload: no type hint
    auto x = env.get_feature("x");

    bool threw = false;
    try {
        x.call("pow", 3);
    } catch (std::exception const& e) {
        threw = true;
        EXPECT_NE(std::string(e.what()).find("no static type information"), std::string::npos);
    }
    EXPECT_TRUE(threw);
}

TEST(state, call_reserved_name_shadowing)
{
    grunk::state grunk;

    // MyScalar has its own "value" member, colliding with Feature's built-in value().
    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors(
        [](double v) { return MyScalar(v); }
    )
    .add_member_function("value", &MyScalar::value);

    auto x = grunk.feature(MyScalar(2.));

    grunk::object result = x.call("value");
    ASSERT_TRUE(result.is<MyScalar>());
    EXPECT_NEAR(result.as<MyScalar>().value(), 2., 1e-14);
}

/*TODO: this should ideally fail (non-const member function as action)
TEST(state, usertype_nonconst_method_as_action_lua)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar",
        sol::constructors<MyScalar(double)>(),
        "set", &MyScalar::set
    );

    grunk.eval(R"(
        local x = MyScalar.new_feature(2.)
        MyScalar.set(x, 13.)
        x:as(MyScalar).set(13.)
    )");
}
*/

/*

TO DO
  - test (nested) enums
  - test data member as action (read-only) in LUA and C++
  - idea to prevent non-const member functions:
      - wrap registration of method in TypeFactory like in reflect, with a add_member_function method
      - use metaprogramming alchemistry to determine if argument to add_member_function is non-const member function
      - if yes, register a method that throws an exception or returns an invalid sol::protected_function_result
  - docstrings + documentation
  - copy potentially missing tests from main branch
  - think about good syntax for scripts and expressions (having mixed yaml-lua in mind)
     - Idea: In the recipe, have a block "exports" which contains a LUA script as a string. LUA functions and 
       "LUA classes" will be made available for use in the recipe.
     - In the future, this could be used for LUA plugins: A LUA plugin is just a recipe, where the steps are ignored
       and the exports are the plugin functionality
  - copy plugin interface
  - conan test_package and plugin tests in gtest
  - code generator
  - check smart and custom pointer support
  - static actions taking dynamic features

*/
