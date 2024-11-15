#include <gtest/gtest.h>
#include <state.hpp>


TEST(operators, addition_object_cpp)
{
    grunk::state grunk;
    grunk.set_functions_are_actions(false); // This is needed such that LUA code is not decorated as actions, but are used as is
    using grunk::operator+;                 // This is needed for name-dependent lookup of the operator for grunk::object, which is just a typedef
    grunk.eval(R"(
    x = 1.2
    y = 1.3
    )");
    grunk::object x = grunk["x"];
    grunk::object y = grunk["y"];
    auto z = x + y;
    EXPECT_NEAR(z.as<double>(), 2.5, 1e-14);


    // now wrapped in a dynamic Feature;
    auto fx = grunk::feature(x);
    auto fy = grunk::feature(y);
    auto fz = fx + fy;
    EXPECT_NEAR(fz.value().as<double>(), 2.5, 1e-14);
    fx.set_value(y);
    EXPECT_NEAR(fz.value().as<double>(), 2.6, 1e-14);
}

TEST(operators, subtraction_object_cpp)
{
    grunk::state grunk;
    grunk.set_functions_are_actions(false); // This is needed such that LUA code is not decorated as actions, but are used as is
    using grunk::operator-;                 // This is needed for name-dependent lookup of the operator for grunk::object, which is just a typedef
    grunk.eval(R"(
    x = 1.9
    y = 1.1
    )");
    grunk::object x = grunk["x"];
    grunk::object y = grunk["y"];
    auto z = x - y;
    EXPECT_NEAR(z.as<double>(), 0.8, 1e-14);


    // now wrapped in a dynamic Feature;
    auto fx = grunk::feature(x);
    auto fy = grunk::feature(y);
    auto fz = fx - fy;
    EXPECT_NEAR(fz.value().as<double>(), 0.8, 1e-14);
    fx.set_value(y);
    EXPECT_NEAR(fz.value().as<double>(), 0.0, 1e-14);
}

TEST(operators, multiplication_object_cpp)
{
    grunk::state grunk;
    grunk.set_functions_are_actions(false); // This is needed such that LUA code is not decorated as actions, but are used as is
    using grunk::operator*;                 // This is needed for name-dependent lookup of the operator for grunk::object, which is just a typedef
    grunk.eval(R"(
    x = 4
    y = 5
    )");
    grunk::object x = grunk["x"];
    grunk::object y = grunk["y"];
    auto z = x * y;
    EXPECT_NEAR(z.as<double>(), 20, 1e-14);


    // now wrapped in a dynamic Feature;
    auto fx = grunk::feature(x);
    auto fy = grunk::feature(y);
    auto fz = fx * fy;
    EXPECT_NEAR(fz.value().as<double>(), 20, 1e-14);
    fx.set_value(y);
    EXPECT_NEAR(fz.value().as<double>(), 25, 1e-14);
}

TEST(operators, division_object_cpp)
{
    grunk::state grunk;
    grunk.set_functions_are_actions(false); // This is needed such that LUA code is not decorated as actions, but are used as is
    using grunk::operator/;                 // This is needed for name-dependent lookup of the operator for grunk::object, which is just a typedef
    grunk.eval(R"(
    x = 6
    y = 3
    )");
    grunk::object x = grunk["x"];
    grunk::object y = grunk["y"];
    auto z = x / y;
    EXPECT_NEAR(z.as<double>(), 2, 1e-14);


    // now wrapped in a dynamic Feature;
    auto fx = grunk::feature(x);
    auto fy = grunk::feature(y);
    auto fz = fx / fy;
    EXPECT_NEAR(fz.value().as<double>(), 2, 1e-14);
    fx.set_value(y);
    EXPECT_NEAR(fz.value().as<double>(), 1, 1e-14);
}

TEST(operators, pow_object_cpp)
{
    grunk::state grunk;
    grunk.set_functions_are_actions(false); // This is needed such that LUA code is not decorated as actions, but are used as is
    grunk.eval(R"(
    x = 2
    y = 3
    )");
    grunk::object x = grunk["x"];
    grunk::object y = grunk["y"];
    auto z = grunk::pow(x, y);
    EXPECT_NEAR(z.as<double>(), 8, 1e-14);


    // now wrapped in a dynamic Feature;
    auto fx = grunk::feature(x);
    auto fy = grunk::feature(y);
    auto fz = grunk::pow(fx, fy);
    EXPECT_NEAR(fz.value().as<double>(), 8, 1e-14);
    fx.set_value(y);
    EXPECT_NEAR(fz.value().as<double>(), 27, 1e-14);
}

TEST(operators, modulo_object_cpp)
{
    grunk::state grunk;
    grunk.set_functions_are_actions(false); // This is needed such that LUA code is not decorated as actions, but are used as is
    using grunk::operator%;                 // This is needed for name-dependent lookup of the operator for grunk::object, which is just a typedef
    grunk.eval(R"(
    x = 18
    y = 12
    )");
    grunk::object x = grunk["x"];
    grunk::object y = grunk["y"];
    auto z = x % y;
    EXPECT_NEAR(z.as<double>(), 6, 1e-14);


    // now wrapped in a dynamic Feature;
    auto fx = grunk::feature(x);
    auto fy = grunk::feature(y);
    auto fz = fx % fy;
    EXPECT_NEAR(fz.value().as<double>(), 6, 1e-14);
    fx.set_value(y);
    EXPECT_NEAR(fz.value().as<double>(), 0, 1e-14);
}

TEST(operators, unm_object_cpp)
{
    grunk::state grunk;
    grunk.set_functions_are_actions(false); // This is needed such that LUA code is not decorated as actions, but are used as is
    using grunk::operator-;                 // This is needed for name-dependent lookup of the operator for grunk::object, which is just a typedef
    grunk.eval(R"(
    x = 5
    y = -3
    )");
    grunk::object x = grunk["x"];
    grunk::object y = grunk["y"];
    auto z = - x;
    EXPECT_NEAR(z.as<double>(), -5, 1e-14);


    // now wrapped in a dynamic Feature;
    auto fx = grunk::feature(x);
    auto fy = grunk::feature(y);
    auto fz = -fx;
    EXPECT_NEAR(fz.value().as<double>(), -5, 1e-14);
    fx.set_value(y);
    EXPECT_NEAR(fz.value().as<double>(), 3, 1e-14);
}

TEST(operators, chaining_object_cpp)
{
    grunk::state grunk;
    grunk.set_functions_are_actions(false); // This is needed such that LUA code is not decorated as actions, but are used as is
    using namespace grunk;                  // This is needed for name-dependent lookup of the operator for grunk::object, which is just a typedef
    grunk.eval(R"(
    x = 4
    y = 3
    two = 2
    )");
    grunk::object x = grunk["x"];
    grunk::object y = grunk["y"];
    grunk::object two = grunk["two"];
    auto z =  -(x+y)/(pow(x, two) + pow(y, two));
    EXPECT_NEAR(z.as<double>(), -7./25., 1e-14);


    // now wrapped in a dynamic Feature;
    auto fx = grunk::feature(x);
    auto fy = grunk::feature(y);
    auto ftwo = grunk::feature(two);
    auto fz = -(fx+fy)/(pow(fx, ftwo) + pow(fy, ftwo));
    EXPECT_NEAR(fz.value().as<double>(),  -7./25., 1e-14);
    fx.set_value(y);
    EXPECT_NEAR(fz.value().as<double>(), -6./18., 1e-14);
}

TEST(operators, addition_cpp)
{
    auto x = grunk::Feature(2.);
    auto y = grunk::Feature(3.);
    auto z = x+y;
    EXPECT_NEAR(z.value(), 5., 1e-14);
    x.set_value(3.);
    EXPECT_NEAR(z.value(), 6., 1e-14);
}

TEST(operators, subtraction_cpp)
{
    auto x = grunk::Feature(3.);
    auto y = grunk::Feature(2.);
    auto z = x-y;
    EXPECT_NEAR(z.value(), 1., 1e-14);
    x.set_value(4.);
    EXPECT_NEAR(z.value(), 2., 1e-14);
}

TEST(operators, multiplication_cpp)
{
    auto x = grunk::Feature(3.);
    auto y = grunk::Feature(2.);
    auto z = x*y;
    EXPECT_NEAR(z.value(), 6., 1e-14);
    x.set_value(4.);
    EXPECT_NEAR(z.value(), 8., 1e-14);
}

TEST(operators, division_cpp)
{
    auto x = grunk::Feature(6.);
    auto y = grunk::Feature(3.);
    auto z = x/y;
    EXPECT_NEAR(z.value(), 2., 1e-14);
    x.set_value(4.);
    EXPECT_NEAR(z.value(), 4./3, 1e-14);
}

TEST(operators, pow_cpp)
{
    auto x = grunk::Feature(2.); //TODO: Why doesn't auto-invalidation work if I write grunk::Feature(2) here?
    auto y = grunk::Feature(3);
    auto z = grunk::pow(x, y);
    EXPECT_NEAR(z.value(), 8, 1e-14);
    x.set_value(3);
    EXPECT_NEAR(z.value(), 27, 1e-14);
}

TEST(operators, mod_cpp)
{
    auto x = grunk::Feature(18);
    auto y = grunk::Feature(12);
    auto z = x % y;
    EXPECT_NEAR(z.value(), 6, 1e-14);
    x.set_value(14);
    EXPECT_NEAR(z.value(), 2, 1e-14);
}


TEST(operators, unm_cpp)
{
    auto x = grunk::Feature(5);
    auto z = -x;
    EXPECT_NEAR(z.value(), -5, 1e-14);
    x.set_value(-3);
    EXPECT_NEAR(z.value(), 3, 1e-14);
}

TEST(operators, chaining_cpp)
{
    auto x = grunk::Feature(4.);
    auto y = grunk::Feature(3.);
    auto exp = grunk::Feature(2);
    auto z = -(x+y)/(pow(x, exp) + pow(y, exp));
    EXPECT_NEAR(z.value(), -7./25., 1e-14);
    x.set_value(3.);
    EXPECT_NEAR(z.value(), -6./18., 1e-14);
}

// TODO: operators that implicitly convert constants to Feature<T>

TEST(operators, addition_lua)
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
    EXPECT_NEAR(grunk["z1"].as<double>(), 3., 1e-14);
    EXPECT_NEAR(grunk["z2"].as<double>(), 4., 1e-14);
}

TEST(operators, subtraction_lua)
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
    EXPECT_NEAR(grunk["z1"].as<double>(), -1., 1e-14);
    EXPECT_NEAR(grunk["z2"].as<double>(),  0., 1e-14);
}

TEST(operators, multiplication_lua)
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
    EXPECT_NEAR(grunk["z1"].as<double>(), 2., 1e-14);
    EXPECT_NEAR(grunk["z2"].as<double>(), 4., 1e-14);
}

TEST(operators, division_lua)
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
    EXPECT_NEAR(grunk["z1"].as<double>(), 0.5, 1e-14);
    EXPECT_NEAR(grunk["z2"].as<double>(), 1. , 1e-14);
}

TEST(operators, modulo_lua)
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
    EXPECT_EQ(grunk["z1"].as<int>(), 17);
    EXPECT_EQ(grunk["z2"].as<int>(),  9);
}

TEST(operators, pow_lua)
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
    EXPECT_NEAR(grunk["z1"].as<double>(),  8, 1e-14);
    EXPECT_NEAR(grunk["z2"].as<double>(), 27, 1e-14);
}

TEST(operators, unm_lua)
{
    grunk::state grunk;

    grunk.eval(R"(
        local x = grunk.feature(2.)
        local z = -x
        z1 = z:value()
        x:set_value(3.)
        z2 = z:value()
    )");
    EXPECT_NEAR(grunk["z1"].as<double>(), -2, 1e-14);
    EXPECT_NEAR(grunk["z2"].as<double>(), -3, 1e-14);
}

TEST(operators, chaining_lua)
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
    EXPECT_NEAR(grunk["z1"].as<double>(), -7./25., 1e-14);
    EXPECT_NEAR(grunk["z2"].as<double>(), -6./18., 1e-14);
}

namespace {

class MyScalar {
public:
    MyScalar(double v) : m_value(v) {}
    double value() const { return m_value; }
    void set(double v) { m_value = v; }

private:
    double m_value;
};

MyScalar operator+(MyScalar const& l, MyScalar const& r) {
    return {l.value() + r.value()};
}

} // anonymous namespace


TEST(operators, usertype_addition_lua)
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

    EXPECT_NEAR(grunk["z1"].as<MyScalar>().value(), 5., 1e-14);
    EXPECT_NEAR(grunk["z2"].as<MyScalar>().value(), 42., 1e-14);
}
