// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
#include <grunk/recipe.hpp>

TEST(Recipe, module_call_and_reactive_invalidation)
{
    grunk::state grunk;

    auto recipe = grunk.create_recipe();
    auto x = grunk.feature(1.).with_id("x");
    recipe["x"] = x;

    recipe.insert_module_script(
        "mymod",
        "function inc(a) return a + 1 end"
    );

    recipe.eval("y = mymod.inc(x)");

    auto y = recipe.get_feature("y");
    EXPECT_NEAR(y.value().as<double>(), 2., 1e-15);

    // editing the parameter invalidates the module call, like any other dependency
    x.set_value(10.);
    EXPECT_FALSE(y.is_valid());
    EXPECT_NEAR(y.value().as<double>(), 11., 1e-15);

    // editing the module's *source code* also invalidates every call made into it
    recipe.module_scripts.at("mymod").set_value("function inc(a) return a + 100 end");
    EXPECT_FALSE(y.is_valid());
    EXPECT_NEAR(y.value().as<double>(), 110., 1e-15);
}

TEST(Recipe, module_yaml_roundtrip)
{
    std::string serialized;

    {
        grunk::state grunk;
        auto recipe = grunk.create_recipe();

        auto x = grunk.feature(4.).with_id("x");
        recipe["x"] = x;

        recipe.insert_module_script(
            "mymod",
            R"(
function inc(a)
    return a + 1
end
)"
        );

        recipe.eval("y = mymod.inc(x)");
        recipe.tag();

        serialized = recipe.to_string();
        EXPECT_NE(serialized.find("modules:"), std::string::npos);
        EXPECT_NE(serialized.find("mymod.inc(x)"), std::string::npos);
    }

    grunk::state grunk;
    auto recipe = grunk.create_recipe();
    recipe.populate_from_string(serialized);

    ASSERT_EQ(recipe.module_scripts.size(), 1u);
    auto y = recipe.get_feature("y");
    EXPECT_NEAR(y.value().as<double>(), 5., 1e-15);

    // the reloaded module is still live: editing its script still invalidates y
    recipe.module_scripts.at("mymod").set_value("function inc(a) return a + 41 end");
    EXPECT_NEAR(y.value().as<double>(), 45., 1e-15);
}

TEST(Recipe, module_clone_independence)
{
    grunk::state grunk;

    auto recipe = grunk.create_recipe();
    auto x = grunk.feature(1.).with_id("x");
    recipe["x"] = x;
    recipe.insert_module_script("mymod", "function inc(a) return a + 1 end");
    recipe.eval("y = mymod.inc(x)");

    ASSERT_NEAR(recipe.get_feature("y").value().as<double>(), 2., 1e-15);

    auto clone = recipe.clone();
    EXPECT_NEAR(clone.get_feature("y").value().as<double>(), 2., 1e-15);

    // editing the clone's module script does not affect the source recipe
    clone.module_scripts.at("mymod").set_value("function inc(a) return a + 1000 end");
    EXPECT_NEAR(clone.get_feature("y").value().as<double>(), 1001., 1e-15);
    EXPECT_NEAR(recipe.get_feature("y").value().as<double>(), 2., 1e-15);

    // editing the source recipe's module script does not affect the clone
    recipe.module_scripts.at("mymod").set_value("function inc(a) return a + 7 end");
    EXPECT_NEAR(recipe.get_feature("y").value().as<double>(), 8., 1e-15);
    EXPECT_NEAR(clone.get_feature("y").value().as<double>(), 1001., 1e-15);
}

TEST(Recipe, module_multi_function_nested_calls)
{
    // Mirrors the canonical example from the "Implement a Module Action" issue: a module with
    // several functions, called from steps with a nested call as an argument.
    std::string const module_script = R"(
function less_than(a, b)
    return a<b
end

function if_then_else(cond, branch1, branch2)
    if cond then
        return branch1
    else
        return branch2
    end
end
)";

    std::string serialized;
    {
        grunk::state grunk;
        auto recipe = grunk.create_recipe();

        auto a = grunk.feature(2.).with_id("a");
        auto b = grunk.feature(11.).with_id("b");
        recipe["a"] = a;
        recipe["b"] = b;

        recipe.insert_module_script("mod", module_script);
        recipe.eval("c = mod.if_then_else(mod.less_than(a,2), a, b)");
        recipe.tag();

        auto c = recipe.get_feature("c");
        EXPECT_NEAR(c.value().as<double>(), 11., 1e-15);

        // a < 2 is now true, so if_then_else should switch branches
        a.set_value(1.);
        EXPECT_FALSE(c.is_valid());
        EXPECT_NEAR(c.value().as<double>(), 1., 1e-15);

        serialized = recipe.to_string();
        EXPECT_NE(serialized.find("modules:"), std::string::npos);
        EXPECT_NE(serialized.find("c = mod.if_then_else(mod.less_than(a, 2), a, b)"), std::string::npos);
    }

    // round-trip through YAML: the nested calls must still resolve correctly
    grunk::state grunk;
    auto recipe = grunk.create_recipe();
    recipe.populate_from_string(serialized);

    auto c = recipe.get_feature("c");
    EXPECT_NEAR(c.value().as<double>(), 1., 1e-15);

    recipe.get_feature("a").set_value(5.);
    EXPECT_NEAR(c.value().as<double>(), 11., 1e-15);
}

TEST(Recipe, module_isolated_between_recipes)
{
    // Two recipes sharing the same grunk::state and the same module name must not collide:
    // module scripts belong to the recipe, not to the shared state.
    grunk::state grunk;

    auto recipe_a = grunk.create_recipe();
    auto a_x = grunk.feature(1.).with_id("x");
    recipe_a["x"] = a_x;
    recipe_a.insert_module_script("mymod", "function calc(a) return a + 1 end");
    recipe_a.eval("y = mymod.calc(x)");

    auto recipe_b = grunk.create_recipe();
    auto b_x = grunk.feature(1.).with_id("x");
    recipe_b["x"] = b_x;
    recipe_b.insert_module_script("mymod", "function calc(a) return a * 10 end");
    recipe_b.eval("y = mymod.calc(x)");

    EXPECT_NEAR(recipe_a.get_feature("y").value().as<double>(), 2., 1e-15);
    EXPECT_NEAR(recipe_b.get_feature("y").value().as<double>(), 10., 1e-15);
}
