#include <gtest/gtest.h>
#include <state.hpp>

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

TEST(state, free_function_lua_feature_user_type)
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

TEST(state, lua_operators_as_action_addition)
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

TEST(state, lua_operators_as_action_subtraction)
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

TEST(state, lua_operators_as_action_multiplication)
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

TEST(state, lua_operators_as_action_division)
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

TEST(state, lua_operators_as_action_modulo)
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

TEST(state, lua_operators_as_action_pow)
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

TEST(state, lua_operators_as_action_unm)
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

TEST(state, lua_operators_as_action_chaining)
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

TEST(state, usertype_lua_ctor_as_action)
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

TEST(state, usertype_lua_operators_as_action)
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

TEST(state, usertype_lua_nonconst_memberfunction)
{
    grunk::state grunk;

    grunk.register_type<MyScalar>("MyScalar",
        sol::constructors<MyScalar(double)>(),
        "set", &MyScalar::set
    );

    grunk.eval(R"(
        local x = MyScalar.new_feature(2.)
        -- x:set(13) would be the cooler syntax. TODO: Make it possible!
        MyScalar.set(x, grunk.feature(13.))
    )");
}

//TO DO
//  - enable easy syntax of calling methods on features containing class instances
//  - test member functions as action
//  - test data member as action
//  - test usertype actions also from cpp
//  - docstrings
//  - decorate math functions?
//  - move this to a branch of grunk with CI
//  - add option to grunk::eval to ammend variable names as feature ids after evaluation
//  - idea for code structure:
//      - header-only core library (optionally with parallelization)
//      - option for plugins (requires boost) and recipes
//      - option for python bindings
//  - add grunk::Recipe class with serialization to mixed yaml and lua
//  - think about good syntax for scripts and expressions
//  - registration syntax as before with reflect
//  - copy plugin interface
//  - python bindings
//  - code generator
//  - static actions taking dynamic features
//  - parallelization with option to disable
