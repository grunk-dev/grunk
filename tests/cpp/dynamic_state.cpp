// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
#include <cmath>

namespace {

    double add(double l, double r) {
        return l + r;
    }

} // anonymous namespace

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
  - add option to grunk::eval to ammend variable names as feature ids after evaluation
  - add grunk::Recipe class with serialization to mixed yaml and lua
  - registration syntax as before with reflect
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
  - python bindings
  - static actions taking dynamic features
  - parallelization with option to disable

*/
