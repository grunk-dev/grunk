#include <gtest/gtest.h>
#include <state.hpp>
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
    grunk.set_functions_are_actions(false);
    grunk.eval(
        R"(
        x = 17
        y = 25
        z = add(x,y)
        )"
    );
    double z = grunk.get("z").as<double>();
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

    grunk.eval(R"(
       x = grunk.feature(2.)
       y = grunk.feature(1.)
       z = add(x,y)
    )");


    auto x = grunk.get_feature("x");
    auto y = grunk.get_feature("y");
    auto z = grunk.get_feature("z");

    EXPECT_FALSE(z.is_valid());
    double zv = z.value().as<double>();
    EXPECT_TRUE(z.is_valid());
    ASSERT_NEAR(zv, 3., 1e-14);

    x.set_value(5.);

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(grunk.get_feature("x").value().as<double>(), 5., 1e-14);
    EXPECT_NEAR(x.value().as<double>(), 5., 1e-14);
    zv = z.value().as<double>();
    EXPECT_TRUE(z.is_valid());
    ASSERT_NEAR(zv, 6., 1e-14);
}

TEST(state, free_function_feature_id_lua)
{
    grunk::state grunk;

    grunk.register_function("add", &add);

    grunk.eval(R"(

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

    EXPECT_EQ(grunk.get_feature("x").id(), "x");
    EXPECT_EQ(grunk.get_feature("y").id(), "y");
    EXPECT_EQ(grunk.get("z_id1").as<std::string>(), "z");
    EXPECT_EQ(grunk.get("z_id2").as<std::string>(), "horst");

    EXPECT_NEAR(grunk.get("z_value1").as<double>(), 3., 1e-14);
    EXPECT_NEAR(grunk.get("z_value2").as<double>(), 43., 1e-14);

}

TEST(state, operators_as_action_addition_lua)
{
    grunk::state grunk;

    grunk.eval(R"(
        local x = grunk.feature(1.)
        local y = grunk.feature(2.)
        local z = x + y
        z1 = z:value()
        x:set_value(2.)
        z2 = z:value()
    )");
    EXPECT_NEAR(grunk.get("z1").as<double>(), 3., 1e-14);
    EXPECT_NEAR(grunk.get("z2").as<double>(), 4., 1e-14);
}

TEST(state, operators_as_action_subtraction_lua)
{
    grunk::state grunk;

    grunk.eval(R"(
        local x = grunk.feature(1.)
        local y = grunk.feature(2.)
        local z = x - y
        z1 = z:value()
        x:set_value(2.)
        z2 = z:value()
    )");
    EXPECT_NEAR(grunk.get("z1").as<double>(), -1., 1e-14);
    EXPECT_NEAR(grunk.get("z2").as<double>(),  0., 1e-14);
}

TEST(state, operators_as_action_multiplication_lua)
{
    grunk::state grunk;

    grunk.eval(R"(
        local x = grunk.feature(1.)
        local y = grunk.feature(2.)
        local z = x * y
        z1 = z:value()
        x:set_value(2.)
        z2 = z:value()
    )");
    EXPECT_NEAR(grunk.get("z1").as<double>(), 2., 1e-14);
    EXPECT_NEAR(grunk.get("z2").as<double>(), 4., 1e-14);
}

TEST(state, operators_as_action_division_lua)
{
    grunk::state grunk;

    grunk.eval(R"(
        local x = grunk.feature(1.)
        local y = grunk.feature(2.)
        local z = x / y
        z1 = z:value()
        x:set_value(2.)
        z2 = z:value()
    )");
    EXPECT_NEAR(grunk.get("z1").as<double>(), 0.5, 1e-14);
    EXPECT_NEAR(grunk.get("z2").as<double>(), 1. , 1e-14);
}

TEST(state, operators_as_action_modulo_lua)
{
    grunk::state grunk;

    grunk.eval(R"(
        local x = grunk.feature(17)
        local y = grunk.feature(24)
        local z = x % y
        z1 = z:value()
        x:set_value(33)
        z2 = z:value()
    )");
    EXPECT_EQ(grunk.get("z1").as<int>(), 17);
    EXPECT_EQ(grunk.get("z2").as<int>(),  9);
}

TEST(state, operators_as_action_pow_lua)
{
    grunk::state grunk;

    grunk.eval(R"(
        local x = grunk.feature(2.)
        local y = grunk.feature(3)
        local z = x ^ y
        z1 = z:value()
        x:set_value(3.)
        z2 = z:value()
    )");
    EXPECT_NEAR(grunk.get("z1").as<double>(),  8, 1e-14);
    EXPECT_NEAR(grunk.get("z2").as<double>(), 27, 1e-14);
}

TEST(state, operators_as_action_unm_lua)
{
    grunk::state grunk;

    grunk.eval(R"(
        local x = grunk.feature(2.)
        local z = -x
        z1 = z:value()
        x:set_value(3.)
        z2 = z:value()
    )");
    EXPECT_NEAR(grunk.get("z1").as<double>(), -2, 1e-14);
    EXPECT_NEAR(grunk.get("z2").as<double>(), -3, 1e-14);
}

TEST(state, operators_as_action_chaining_lua)
{
    grunk::state grunk;

    grunk.eval(R"(
        local x = grunk.feature(4.)
        local y = grunk.feature(3)
        z = -(x+y)/(x^2 + y^2)
        z1 = z:value()
        x:set_value(3.)
        z2 = z:value()
    )");
    EXPECT_NEAR(grunk.get("z1").as<double>(), -7./25., 1e-14);
    EXPECT_NEAR(grunk.get("z2").as<double>(), -6./18., 1e-14);
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

} // anonymous namespace

TEST(state, usertype_ctor_as_action_cpp)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar",
        sol::constructors<MyScalar(double)>()
    );

    //TODO:
    // - 1. ctor as action with named feature arguments
    // - 2. ctor as action with unnamed/constant feature arguments
    // - 3. invoke ctor for creating independent input feature

    ASSERT_TRUE(false);
}

TEST(state, usertype_ctor_as_action_lua)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar",
        sol::constructors<MyScalar(double)>()
    );

    grunk.eval(R"(
        local a = grunk.feature(1.)

        x = MyScalar.new(a)             -- ctor as action: x depends on a
        y = MyScalar.new(2.)            -- ctor as action with argument conversion from constant: y depends on Feature(2.)
        z = MyScalar.new_feature(3.)    -- forwards ctor args to grunk::feature: z is independent feature

        x1 = x:value()
        a:set_value(4.)
        x2 = x:value()
    )");

    EXPECT_EQ(grunk.get_feature("x").node_pointer()->get_parents().size(), 1);
    EXPECT_EQ(grunk.get_feature("y").node_pointer()->get_parents().size(), 1);
    EXPECT_EQ(grunk.get_feature("z").node_pointer()->get_parents().size(), 0);

    EXPECT_EQ(grunk.get_feature("y").value().as<MyScalar>().value(), 2.);
    EXPECT_EQ(grunk.get_feature("z").value().as<MyScalar>().value(), 3.);

    EXPECT_EQ(grunk.get("x1").as<MyScalar>().value(), 1.);
    EXPECT_EQ(grunk.get("x2").as<MyScalar>().value(), 4.);
    EXPECT_EQ(grunk.get_feature("x").value().as<MyScalar>().value(), 4.);
}

TEST(state, usertype_operators_as_action_lua)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar",
        sol::constructors<MyScalar(double)>(),
        "__add", [](MyScalar const& l, MyScalar const&r){
            return l + r;
        },
        "value", &MyScalar::value,
        "set", &MyScalar::set
    );

    grunk.eval(R"(
        local x = MyScalar.new_feature(2.)
        local y = MyScalar.new_feature(3.)
        local z = x + y

        z1 = z:value()

        x:change_value():set(39.)

        z2 = z:value()
    )");

    EXPECT_NEAR(grunk.get("z1").as<MyScalar>().value(), 5., 1e-14);
    EXPECT_NEAR(grunk.get("z2").as<MyScalar>().value(), 42., 1e-14);
}

TEST(state, usertype_method_as_action_cpp)
{
    // TODO
    ASSERT_TRUE(false);
}


TEST(state, usertype_method_as_action_lua)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar",
        sol::constructors<MyScalar(double)>(),
        "pow", &MyScalar::pow,
        "set", &MyScalar::set
    );

    grunk.eval(R"(
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

    EXPECT_NEAR(grunk.get("y1").as<MyScalar>().value(), 4, 1e-14); // 2^2
    EXPECT_NEAR(grunk.get("y2").as<MyScalar>().value(), 9, 1e-14); // 3^2

    EXPECT_NEAR(grunk.get("z1").as<MyScalar>().value(),27, 1e-14); // 3^3
    EXPECT_NEAR(grunk.get("z2").as<MyScalar>().value(), 8, 1e-14); // 2^3
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
 *
TO DO
  - fix windows CI
  - test ctor functions also in C++ API
  - test member functions as action
  - test data member as action (read-only)
  - support operators in C++ API also?
  - test usertype actions also from cpp
  - docstrings
  - think about good syntax for scripts and expressions
  - add option to grunk::eval to ammend variable names as feature ids after evaluation
  - add grunk::Recipe class with serialization to mixed yaml and lua
  - registration syntax as before with reflect
  - idea to prevent non-const member functions:
      - wrap registration of method in TypeFactory like in reflect, with a add_member_function method
      - use metaprogramming alchemistry to determine if argument to add_member_function is non-const member function
      - if yes, register a method that throws an exception or returns an invalid sol::protected_function_result
  - copy plugin interface
  - at least function introspection to get default values and parameter names?
  - code generator
  - python bindings
  - static actions taking dynamic features
  - parallelization with option to disable
  - decorate math functions?
  - enable easy syntax of calling methods on features containing class instances
      - e.g. `x:set(42)` instead of `MyScalar.set(x, 42)`
      - e.g. `x:method(MyScalar.set)(39)
  - idea for code structure:
      - header-only core library (optionally with parallelization)
      - option for plugins (requires boost) and recipes
      - option for python bindings

*/
