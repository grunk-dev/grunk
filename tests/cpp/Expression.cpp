#include <gtest/gtest.h>

#include <grunk/grunk.hpp>

using namespace grunk;

class ExpressionTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        grunk::init();
        
    } 

    static void TearDownTestCase() {
        reflect::get_type_registry().clear();
    } 
};

TEST_F(ExpressionTest, simple)
{
    auto x = Feature("x", "double", 1.5);
    auto y = Feature("y", "double", 2.);
    auto z = grunk::expression("z", "x*y", x,y);
    EXPECT_EQ(z.id(), "z");
    EXPECT_NEAR(z.value().as<double>(), 3., 1e-15);
}

TEST_F(ExpressionTest, serialize)
{
    YAML::Node node;

    {
        auto x = Feature("x", "double", 1.5);
        auto y = Feature("y", "double", 2.);
        auto z = expression("z", "x*y", x,y);

        node = serialize(z);
    } // x, y and z go out of scope here

    EXPECT_EQ(node["steps"].size(), 1);
    EXPECT_EQ(node["steps"][0].Tag(), "expr");
    EXPECT_EQ(node["steps"][0].size(), 2);
    EXPECT_EQ(node["steps"][0][0].as<std::string>(), "z");
    EXPECT_EQ(node["steps"][0][1].as<std::string>(), "x*y");

    auto recipe = Recipe::deserialize(node);
    EXPECT_EQ(recipe.num_features(), 3);
    EXPECT_NEAR(recipe.at("z").value().as<double>(), 3, 1e-15);
}

TEST_F(ExpressionTest, UnknownUnknown)
{
    auto x = Feature("x", "double", 1.5);
    auto y = Feature("y", "double", 2.);
    auto z = expression("z", "a*y", x,y);
    EXPECT_THROW(z.value(), grunk::io_error);
}

TEST_F(ExpressionTest, UnknownFunction)
{
    auto x = Feature("x", "double", 1.5);
    auto y = Feature("y", "double", 2.);
    auto z = expression("z", "x*fun(y)", x,y);
    EXPECT_THROW(z.value(), grunk::io_error);
}

TEST_F(ExpressionTest, UnknownOperator)
{
    auto x = Feature("x", "double", 1.5);
    auto y = Feature("y", "double", 2.);
    auto z = expression("z", "x$y", x,y);
    EXPECT_THROW(z.value(), grunk::io_error);
}

TEST_F(ExpressionTest, EmptyString)
{
    auto x = Feature("x", "double", 1.5);
    auto y = Feature("y", "double", 2.);
    auto z = expression("z", "", x,y);
    EXPECT_THROW(z.value(), grunk::io_error);
}

TEST_F(ExpressionTest, IsThisABug)
{
    //https://gitlab.dlr.de/paradigms/grunk/-/issues/90
    auto x = Feature("x", "double", 1.);
    auto y = expression("y", "0.75*x", x);
    auto z = expression("z", "y/3.", y);
    EXPECT_NEAR(0.25, z.value().as<double>(), 1e-10);
}
