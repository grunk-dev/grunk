#include <gtest/gtest.h>
#include <grunk/recipe/Recipe.hpp>
#include "grunk/version.hpp"

namespace {

double add(double l, double r) { return l+r; }

} // anonymous namespace

TEST(Recipe, simple_primitive_parameters)
{
    grunk::state grunk;
    grunk.register_function("add", &add);
    
    auto x = grunk.feature(1.).with_id("x");
    auto y = grunk.feature(2.).with_id("y");
    auto z = grunk.action("add", x, y).with_id("z");
    auto w = grunk::pow(z, 2).with_id("w");

    auto recipe = grunk::Recipe(grunk);
    recipe.insert("w", w);
    std::string out = "\n" + recipe.to_string();
    std::string expected = R"(
uses:
  grunk: )" grunk_VERSION R"(
parameters:
  x: 1
  y: 2
steps: |
  z = add(x, y)
  w = z ^ 2
)";
    EXPECT_EQ(out, expected);

    recipe.write("test.grr.yml");

    auto recipe2 = grunk::Recipe::from_string(grunk, out);
    auto w2 = recipe2.get_feature("w");
    EXPECT_NEAR(w2.value().as<double>(), 9., 1e-10);
    auto z2 = recipe2.get_feature("z");
    EXPECT_NEAR(z2.value().as<double>(), 3., 1e-10);
    auto y2 = recipe2.get_feature("y");
    EXPECT_NEAR(y2.value().as<double>(), 2., 1e-10);
    auto x2 = recipe2.get_feature("x");
    EXPECT_NEAR(x2.value().as<double>(), 1., 1e-10);
}