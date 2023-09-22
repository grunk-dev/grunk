#include <gtest/gtest.h>

#include <grunk/grunk.hpp>

using namespace grunk;

class RecipeTest : public ::testing::Test
{
public:
    static void SetUpTestCase() {

        reflect::register_type<double>("double")
        .add_constructor<double>();

        reflect::register_function(
            [](double x, double y){ return x+y; }, 
            "add"
        );
    }

    static void TearDownTestCase() {
        reflect::get_type_registry().clear();
    } 
};

TEST_F(RecipeTest, clone)
{
    Feature x("x", "double", 12.3);
    Feature y("y", "double", 29.7);
    Feature z = action("z", "add", x, y).output();
    ASSERT_NEAR(z.value().as<double>(), 42., 1e-15);

    Recipe my_recipe({x, y, z});
    
    Recipe clone = my_recipe.clone();

    EXPECT_EQ(clone.size(), 3);
    EXPECT_EQ(clone.at("x").value().as<double>(), 12.3);
    EXPECT_EQ(clone.at("y").value().as<double>(), 29.7);
    EXPECT_NEAR(clone.at("z").value().as<double>(), 42.0, 1e-15);

    x.access_value() = 13.2;
    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(z.value().as<double>(), 42.9, 1e-15);

    // cloned recipe should be uneffected by changes in source recipe
    EXPECT_TRUE(clone.at("z").is_valid());
    EXPECT_EQ(clone.at("x").value().as<double>(), 12.3);
    EXPECT_NEAR(clone.at("z").value().as<double>(), 42.0, 1e-15);

    clone.at("x").access_value() = 10.;
    EXPECT_FALSE(clone.at("z").is_valid());
    EXPECT_NEAR(clone.at("z").value().as<double>(), 39.7, 1e-15);

    // source recipe should be uneffected by changes in cloned recipe
    EXPECT_TRUE(z.is_valid());
    EXPECT_EQ(x.value().as<double>(), 13.2);
    EXPECT_NEAR(z.value().as<double>(), 42.9, 1e-15);
}

TEST_F(RecipeTest, as_function)
{
    Feature x("x", "double", 12.3);
    Feature y("y", "double", 29.7);
    Feature z = action("z", "add", x, y).output();
    Recipe my_recipe({x, y, z});

    Feature a("a", "double", -10.);
    auto res = my_recipe(
        {{"my_z", "z"}}, // create a new node my_z that contains the value of the inner node z
        {{"x", a}} // map the inner node x to input parameter a
    );
    Feature my_z = res.at("my_z");
    EXPECT_NEAR(my_z.value().as<double>(), 19.7, 1e-15);

    // To Do:
    //  - check id of output
    //  - check that my_recipe nodes are uneffected by function evaluation
}