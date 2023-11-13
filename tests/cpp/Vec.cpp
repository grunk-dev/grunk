#include <gtest/gtest.h>

#include <grunk/grunk.hpp>
#include <numeric>

namespace {

double sum(std::vector<double> const& v){
    return std::accumulate(v.begin(), v.end(), 0.);
}

}

class VecTest : public ::testing::Test
{
public:
    static void SetUpTestCase() {

        grunk::init();

        reflect::register_function(&sum, "sum");

    }

    static void TearDownTestCase() {
        reflect::get_type_registry().clear();
    } 
};

TEST_F(VecTest, static_mode)
{
    auto x = grunk::Feature("x", 0.1);
    auto y = grunk::Feature("y", 0.2);
    auto z = grunk::vec("z", x, y);

    static_assert(std::is_same_v<std::decay_t<decltype(z.value())>, std::vector<double>>);

    EXPECT_EQ(z.value().size(), 2);
    EXPECT_EQ(z.value()[0], 0.1);
    EXPECT_EQ(z.value()[1], 0.2);

    x.access_value() = -0.1;
    EXPECT_FALSE(z.is_valid());
    EXPECT_EQ(z.value()[0], -0.1);
}

TEST_F(VecTest, dynamic_mode)
{
    auto x = grunk::Feature("x", "double", 0.1);
    auto y = grunk::Feature("y", "double", 0.2);
    auto z = grunk::vec("z", x, y);

    static_assert(std::is_same_v<std::decay_t<decltype(z.value())>, reflect::DynamicObject>);

    auto ret = z.value().as<std::vector<reflect::DynamicObject>>();
    EXPECT_EQ(ret.size(), 2);
    EXPECT_EQ(ret[0].as<double>(), 0.1);
    EXPECT_EQ(ret[1].as<double>(), 0.2);

    x.access_value() = -0.1;
    EXPECT_FALSE(z.is_valid());
    ret = z.value().as<std::vector<reflect::DynamicObject>>();
    EXPECT_EQ(ret[0].as<double>(), -0.1);
}

TEST_F(VecTest, dynamic_mode_vector_as_argument)
{
    auto x = grunk::Feature("x", "double", 0.1);
    auto y = grunk::Feature("y", "double", 0.2);
    auto s = grunk::action("s", "sum", grunk::vec("z", x, y)).output();
    EXPECT_NEAR(s.value().as<double>(), 0.3, 1e-15);
}

TEST_F(VecTest, dynamic_mode_serialize)
{
    auto x = grunk::Feature("x", "double", 0.1);
    auto y = grunk::Feature("y", "double", 0.2);
    auto s = grunk::action("s", "sum", grunk::vec("z", x, y)).output();
    YAML::Node n = grunk::serialize(s);

    EXPECT_EQ(n["steps"].size(), 2);
    auto step0 = n["steps"][0];
    EXPECT_EQ(step0.Tag(), "vec");
    EXPECT_EQ(step0.size(), 2);
    EXPECT_EQ(step0[0].size(), 1);
    EXPECT_EQ(step0[0][0].as<std::string>(), "z");
    EXPECT_EQ(step0[1].size(), 2);
    EXPECT_EQ(step0[1][0].as<std::string>(), "x");
    EXPECT_EQ(step0[1][1].as<std::string>(), "y");
}

TEST_F(VecTest, dynamic_mode_deserialize)
{
    YAML::Node n;
    {
        auto x = grunk::Feature("x", "double", 0.1);
        auto y = grunk::Feature("y", "double", 0.2);
        auto s = grunk::action("s", "sum", grunk::vec("z", x, y)).output();
        n = grunk::serialize(s);
    }
    auto recipe = grunk::Recipe::deserialize(n);

    EXPECT_EQ(recipe.num_features(), 4);
    EXPECT_EQ(recipe.num_recipes(), 0);
    EXPECT_NEAR(recipe["s"].value().as<double>(), 0.3, 1e-10);
}

TEST_F(VecTest, dynamic_mode_constants)
{
    {
        auto x = grunk::Feature("", "double", 0.1);
        auto y = grunk::Feature("", "double", 0.2);
        auto s = grunk::action("s", "sum", grunk::vec("z", x, y)).output();
        YAML::Node n = grunk::serialize(s);
        EXPECT_FALSE(n["parameters"]);
        EXPECT_EQ(n["steps"].size(), 2);
        EXPECT_EQ(n["steps"][0].Tag(), "vec");
        EXPECT_EQ(n["steps"][0][1][0].Tag(), "double");
        EXPECT_EQ(n["steps"][0][1][1].Tag(), "double");

        grunk::write("tmp.grr.yml", s);
    }

    auto r = grunk::read("tmp.grr.yml");
    EXPECT_EQ(r.num_features(), 2);
    EXPECT_NEAR(r["s"].value().as<double>(), 0.3, 1e-10);

}