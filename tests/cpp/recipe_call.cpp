#include <gtest/gtest.h>
#include <grunk/dynamic/state.hpp>
#include <grunk/recipe/Recipe.hpp>
#include <grunk/recipe/RecipeCaller.hpp>
#include "grunk/version.hpp"

TEST(Recipe, call_cpp)
{
    grunk::state grunk;

    // create inner recipe
    auto recipe_inner = grunk.create_recipe();
    auto x = grunk.feature(17.);
    auto y = grunk.feature(13.);
    auto z = grunk.feature(2);
    auto w = (x + y) * z;
    recipe_inner["x"] = x;
    recipe_inner["y"] = y;
    recipe_inner["z"] = z;
    recipe_inner["w"] = w;

    // create outer recipe
    auto recipe_outer = grunk.create_recipe();
    auto a = grunk.feature(15.);
    auto b = grunk.feature(11.);
    recipe_outer["a"] = a;
    recipe_outer["b"] = b;
    recipe_outer.insert_recipe("inner", std::move(recipe_inner));

    auto inner = recipe_outer.recipe_caller("inner");
    inner["x"] = a;
    inner["y"] = b;
    auto c = inner.get("w");
    recipe_outer["c"] = c;

    // Check topology of the parametric tree
    EXPECT_EQ(c.node_pointer()->get_children().size(), 0); // c is output
    ASSERT_EQ(c.node_pointer()->get_parents().size(), 1);  // c's parent is a RecipeGetValAction compute node
    auto getvalaction = c.node_pointer()->get_parents()[0];
    ASSERT_EQ(getvalaction->get_parents().size(), 1);  // getvalaction's parent is a Recipe<Feature>
    auto recipe_feature = getvalaction->get_parents()[0];
    ASSERT_EQ(recipe_feature->get_parents().size(), 1);  // recipe_feature's parent is a RecipeAction compute node
    auto recipe_action = recipe_feature->get_parents()[0];
    ASSERT_EQ(recipe_action->get_parents().size(), 3);  // recipe_action's inputs are the inner recipe and a and b
    auto root_recipe = recipe_action->get_parents()[0];
    auto root_a = recipe_action->get_parents()[1];
    auto root_b = recipe_action->get_parents()[2];
    EXPECT_EQ(root_recipe->get_parents().size(), 0);
    EXPECT_EQ(root_a->get_parents().size(), 0);
    EXPECT_EQ(root_b->get_parents().size(), 0);
    
    // c = (a + b) * inner_z = (15 + 11) * 52
    EXPECT_NEAR(c.value().as<double>(), 52, 1e-15);

    // inner recipe should be unaffected (due to deep-copy)
    auto& inner_recipe = recipe_outer.get_recipe("inner").change_value();
    EXPECT_EQ(inner_recipe.get_feature("x").value().as<double>(), 17);
    EXPECT_EQ(inner_recipe.get_feature("y").value().as<double>(), 13);
    EXPECT_EQ(inner_recipe.get_feature("z").value().as<double>(), 2);
    EXPECT_NEAR(inner_recipe.get_feature("w").value().as<double>(), 60, 1e-15);

    // changing inner recipe should invalidate c
    inner_recipe.get_feature("z").set_value(0.5);
    EXPECT_FALSE(c.is_valid());
    EXPECT_NEAR(c.value().as<double>(), 13., 1e-15);

    // inner recipe should be unaffected (due to deep-copy)
    EXPECT_EQ(inner_recipe.get_feature("x").value().as<double>(), 17);
    EXPECT_EQ(inner_recipe.get_feature("y").value().as<double>(), 13);
    EXPECT_EQ(inner_recipe.get_feature("z").value().as<double>(), 0.5);
    EXPECT_NEAR(inner_recipe.get_feature("w").value().as<double>(), 15, 1e-15);
    
}

TEST(Recipe, call_lua)
{
    grunk::state grunk;

    // create inner recipe
    auto recipe_inner = grunk.create_recipe();
    recipe_inner.eval(R"(
        x = grunk.feature(17.)
        y = grunk.feature(13.)
        z = grunk.feature(2.)
        w = (x + y) * z
    )");

    // create outer recipe
    auto recipe_outer = grunk.create_recipe();
    recipe_outer.insert_recipe("inner", std::move(recipe_inner));

    recipe_outer.eval(R"(
        a = grunk.feature(15.)
        b = grunk.feature(11.)
        inner = recipe_caller("inner")
        tmp = inner["x"]
        --inner["x"]:assign(a)
        inner["x"] = a
        inner["y"] = b
        c = inner.get("w")
    )");

    sol::object tmp = recipe_outer["tmp"];
    EXPECT_FALSE(tmp.is<sol::nil_t>());
    // EXPECT_TRUE(tmp.is<grunk::RecipeCaller::Proxy>());
    EXPECT_FALSE(tmp.is<grunk::DynamicFeature>());
    EXPECT_EQ(tmp.get_type(), sol::type::userdata);
    EXPECT_TRUE(tmp.valid());

    sol::object c = recipe_outer["c"];
    EXPECT_FALSE(c.is<sol::nil_t>());
    EXPECT_TRUE(c.is<grunk::DynamicFeature>());

    // auto c = recipe_outer.get_feature("c");
/*    
    // Check topology of the parametric tree
    EXPECT_EQ(c.node_pointer()->get_children().size(), 0); // c is output
    ASSERT_EQ(c.node_pointer()->get_parents().size(), 1);  // c's parent is a RecipeGetValAction compute node
    auto getvalaction = c.node_pointer()->get_parents()[0];
    ASSERT_EQ(getvalaction->get_parents().size(), 1);  // getvalaction's parent is a Recipe<Feature>
    auto recipe_feature = getvalaction->get_parents()[0];
    ASSERT_EQ(recipe_feature->get_parents().size(), 1);  // recipe_feature's parent is a RecipeAction compute node
    auto recipe_action = recipe_feature->get_parents()[0];
    ASSERT_EQ(recipe_action->get_parents().size(), 3);  // recipe_action's inputs are the inner recipe and a and b
    auto root_recipe = recipe_action->get_parents()[0];
    auto root_a = recipe_action->get_parents()[1];
    auto root_b = recipe_action->get_parents()[2];
    EXPECT_EQ(root_recipe->get_parents().size(), 0);
    EXPECT_EQ(root_a->get_parents().size(), 0);
    EXPECT_EQ(root_b->get_parents().size(), 0);
    
    // c = (a + b) * inner_z = (15 + 11) * 52
    EXPECT_NEAR(c.value().as<double>(), 52, 1e-15);

    // inner recipe should be unaffected (due to deep-copy)
    auto& inner_recipe = recipe_outer.get_recipe("inner").change_value();
    EXPECT_EQ(inner_recipe.get_feature("x").value().as<double>(), 17);
    EXPECT_EQ(inner_recipe.get_feature("y").value().as<double>(), 13);
    EXPECT_EQ(inner_recipe.get_feature("z").value().as<double>(), 2);
    EXPECT_NEAR(inner_recipe.get_feature("w").value().as<double>(), 60, 1e-15);

    // changing inner recipe should invalidate c
    inner_recipe.get_feature("z").set_value(0.5);
    EXPECT_FALSE(c.is_valid());
    EXPECT_NEAR(c.value().as<double>(), 13., 1e-15);

    // inner recipe should be unaffected (due to deep-copy)
    EXPECT_EQ(inner_recipe.get_feature("x").value().as<double>(), 17);
    EXPECT_EQ(inner_recipe.get_feature("y").value().as<double>(), 13);
    EXPECT_EQ(inner_recipe.get_feature("z").value().as<double>(), 0.5);
    EXPECT_NEAR(inner_recipe.get_feature("w").value().as<double>(), 15, 1e-15);
*/
}