#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
#include <grunk/recipe.hpp>

namespace {
    double add(double l, double r) { return l + r; };
}

TEST(Recipe, clone)
{
    grunk::state grunk;
    grunk.register_function("add", &add);

    auto x = grunk.feature(12.3).with_id("x");
    auto y = grunk.feature(29.7).with_id("y");
    auto z = grunk.action("add", x, y).with_id("z");

    ASSERT_NEAR(z.value().as<double>(), 42., 1e-15);

    auto my_recipe = grunk.create_recipe();
    my_recipe["x"] = x;
    my_recipe["y"] = y;
    my_recipe["z"] = z;

    auto clone = my_recipe.clone();

    // I expect no rounding errors when copying floats
    EXPECT_EQ(clone.get_feature("x").value().as<double>(), 12.3);
    EXPECT_EQ(clone.get_feature("y").value().as<double>(), 29.7);
    EXPECT_NEAR(clone.get_feature("z").value().as<double>(), 42, 1e-15);

    x.set_value(13.2);
    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(z.value().as<double>(), 42.9, 1e-15);

    // cloned recipe should be uneffected by changes in source recipe
    EXPECT_TRUE(clone.get_feature("z").is_valid());
    EXPECT_EQ(clone.get_feature("x").value().as<double>(), 12.3);
    EXPECT_NEAR(clone.get_feature("z").value().as<double>(), 42.0, 1e-15);

    clone.get_feature("x").set_value(10.);
    EXPECT_FALSE(clone.get_feature("z").is_valid());
    EXPECT_NEAR(clone.get_feature("z").value().as<double>(), 39.7, 1e-15);

    // source recipe should be uneffected by changes in cloned recipe
    EXPECT_TRUE(z.is_valid());
    EXPECT_EQ(x.value().as<double>(), 13.2);
    EXPECT_NEAR(z.value().as<double>(), 42.9, 1e-15);
}

TEST(Recipe, clone_with_subrecipes)
{
    grunk::state grunk;
    grunk.register_function("add", &add);

    // create inner recipe
    auto recipe_inner = grunk.create_recipe();
    auto x = grunk.feature(17.);
    auto y = grunk.feature(11.);
    auto z = grunk.action("add", x, y);
    recipe_inner["x"] = x;
    recipe_inner["y"] = y;
    recipe_inner["z"] = z;

    // create outer recipe
    auto recipe_outer = grunk.create_recipe();
    recipe_outer["a"] = grunk.feature(13.);
    recipe_outer["b"] = grunk.feature(11.);
    recipe_outer.insert_recipe("addition", std::move(recipe_inner));

    // just some quick sanity checks
    auto other = recipe_outer.clone();
    EXPECT_NE(other.get_feature("a").node_pointer(), recipe_outer.get_feature("a").node_pointer());
    EXPECT_EQ(other.get_feature("a").value().as<double>(), 13.);
    EXPECT_NE(other.get_feature("b").node_pointer(), recipe_outer.get_feature("b").node_pointer());
    EXPECT_EQ(other.get_feature("b").value().as<double>(), 11.);

    auto& orig_inner = recipe_outer.get_recipe("addition").change_value();
    auto& other_inner = other.get_recipe("addition").change_value();
    EXPECT_NE(other_inner.get_feature("x").node_pointer(), orig_inner.get_feature("x").node_pointer());
    EXPECT_EQ(other_inner.get_feature("x").value().as<double>(), 17.);
    EXPECT_NE(other_inner.get_feature("y").node_pointer(), orig_inner.get_feature("y").node_pointer());
    EXPECT_EQ(other_inner.get_feature("y").value().as<double>(), 11.);
    EXPECT_NE(other_inner.get_feature("z").node_pointer(), orig_inner.get_feature("z").node_pointer());
    EXPECT_EQ(other_inner.get_feature("z").value().as<double>(), 28.);
}
