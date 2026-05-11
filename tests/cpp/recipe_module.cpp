// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
#include <grunk/recipe.hpp>

TEST(Recipe, module_insertion)
{
    std::string serialized;
    
    {
        grunk::state grunk;
        auto recipe = grunk.create_recipe();

        auto x = grunk.feature(1).with_id("x");
        auto y = grunk.feature(2).with_id("y");
        auto z = (x + y).with_id("z");

        recipe["x"] = x;
        recipe["y"] = y;
        recipe["z"] = z;

        recipe.insert_module_script(
            "mymod",
            R"(
function add_fortytwo(x)
    return x + 42
end
)"
        );

        recipe.insert_module_script(
            "mymod2",
            R"(
function multiply(x,y)
    return x * y
end

function add(x,y)
    return x + y
end
)"
        );

        serialized = recipe.to_string();
    }

    // writing and reading
    grunk::state grunk;
    auto recipe = grunk.create_recipe();
    recipe.populate_from_string(serialized);

    ASSERT_TRUE(recipe.module_scripts.size() == 2);

    std::string expected = R"(
function add_fortytwo(x)
    return x + 42
end
)";
    ASSERT_EQ(recipe.module_scripts["mymod"].value(), expected);

    expected = R"(
function multiply(x,y)
    return x * y
end

function add(x,y)
    return x + y
end
)";
    ASSERT_EQ(recipe.module_scripts["mymod2"].value(), expected);


    // cloning
    auto recipe2 = recipe.clone();
    ASSERT_TRUE(recipe2.module_scripts.size() == 2);

    expected = R"(
function add_fortytwo(x)
    return x + 42
end
)";
    ASSERT_EQ(recipe2.module_scripts["mymod"].value(), expected);

    expected = R"(
function multiply(x,y)
    return x * y
end

function add(x,y)
    return x + y
end
)";
    ASSERT_EQ(recipe2.module_scripts["mymod2"].value(), expected);

}