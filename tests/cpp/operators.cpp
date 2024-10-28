#include <gtest/gtest.h>
#include <state.hpp>

TEST(operators, addition_cpp)
{
    auto x = grunk::Feature(2.);
    auto y = grunk::Feature(3.);
    auto z = x+y;
    EXPECT_NEAR(z.value(), 5., 1e-14);
    x.set_value(3.);
    EXPECT_NEAR(z.value(), 6., 1e-14);
}

// TODO: subtraction, multiplication, division, pow, modulo, unm, chaining

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
