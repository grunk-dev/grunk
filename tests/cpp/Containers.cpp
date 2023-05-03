#include <gtest/gtest.h>

#include <grunk/grunk.hpp>

using namespace grunk;

namespace {
    double max(std::vector<double> const& v)
    {
        return *std::max_element(std::begin(v), std::end(v));
    }
} // namespace 

class ContainerTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        reflect::register_type<double>("double")
        .add_constructor<double>();

        reflect::register_function(&max, "max");
    } 

    static void TearDownTestCase() {
        reflect::get_type_registry().clear();
    } 
};

TEST_F(ContainerTest, vector_static)
{
    auto x = Feature("x", 0.25);
    auto y = Feature("y", 0.5);
    auto v = action(
        "v",
        [](auto... v){ 
            return std::vector<double>({v...});
        },
        x,
        y
    )->output();
    auto z = action("z", &max, v)->output();
    EXPECT_EQ(z.value(), 0.5);
}

TEST_F(ContainerTest, vector_dynamic)
{
    auto x = Feature("x", "double", 0.25);
    auto y = Feature("y", "double", 0.5);
    auto v = action(
        "v",
        [](auto... v){ 
            return std::vector<double>({v...});
        },
        x,
        y
    )->output();
    auto z = action("z", "max", v)->output();
    EXPECT_EQ(z.value().as<double>(), 0.5);
}
