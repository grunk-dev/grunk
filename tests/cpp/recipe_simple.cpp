#include <gtest/gtest.h>
#include <grunk/dynamic/state.hpp>
#include <grunk/recipe/Recipe.hpp>
#include "grunk/version.hpp"

namespace {

double add(double l, double r) { return l+r; }

} // anonymous namespace

TEST(Recipe, empty)
{
    grunk::state grunk;
    auto recipe = grunk.create_recipe();
    std::string out = "\n" + recipe.to_string();
    std::string expected = R"(
uses:
  grunk: )" grunk_VERSION "\n";
    EXPECT_EQ(out, expected);
}

TEST(Recipe, no_steps)
{
    grunk::state grunk;
    {
        auto x = grunk.feature(1.).with_id("x");
        auto y = grunk.feature(2.).with_id("y");

        auto recipe = grunk.create_recipe();
        recipe["x"] = x;
        recipe["y"] = y;

        std::string out = "\n" + recipe.to_string();
        std::string expected = R"(
uses:
  grunk: )" grunk_VERSION R"(
parameters:
  x: 1.0
  y: 2.0
)";
        EXPECT_EQ(out, expected);

        grunk.write("test.grr.yml", recipe);
    }

    auto recipe = grunk.read("test.grr.yml");
    auto y = recipe.get_feature("y");
    EXPECT_NEAR(y.value().as<double>(), 2., 1e-10);
    auto x = recipe.get_feature("x");
    EXPECT_NEAR(x.value().as<double>(), 1., 1e-10);
}

TEST(Recipe, simple_primitive_parameters)
{
    grunk::state grunk;
    grunk.register_function("add", &add);
 
    {
        auto x = grunk.feature(1.).with_id("x");
        auto y = grunk.feature(2.).with_id("y");
        auto z = grunk.action("add", x, y).with_id("z");

        auto recipe = grunk.create_recipe();
        recipe["w"] = grunk::pow(z, 2).with_id("w");

        std::string out = "\n" + recipe.to_string();
        std::string expected = R"(
uses:
  grunk: )" grunk_VERSION R"(
parameters:
  x: 1.0
  y: 2.0
steps: |
  z = add(x, y)
  w = z ^ 2
)";
        EXPECT_EQ(out, expected);

        grunk.write("test.grr.yml", recipe);
    }

    auto recipe2 = grunk.read("test.grr.yml");
    auto w2 = recipe2.get_feature("w");
    EXPECT_NEAR(w2.value().as<double>(), 9., 1e-10);
    auto z2 = recipe2.get_feature("z");
    EXPECT_NEAR(z2.value().as<double>(), 3., 1e-10);
    auto y2 = recipe2.get_feature("y");
    EXPECT_NEAR(y2.value().as<double>(), 2., 1e-10);
    auto x2 = recipe2.get_feature("x");
    EXPECT_NEAR(x2.value().as<double>(), 1., 1e-10);
}

TEST(Recipe, simple_primitive_parameters_anonymous)
{
    grunk::state grunk;
    grunk.register_function("add", &add);
 
    {
        auto x = grunk.feature(1.).with_id("x");
        auto y = grunk.feature(2.).with_id("y");
        auto z = grunk.action("add", x, 2);

        auto recipe = grunk.create_recipe();
        recipe["w"] = grunk::pow(z, y).with_id("w");

        std::string out = "\n" + recipe.to_string();
        std::string expected = R"(
uses:
  grunk: )" grunk_VERSION R"(
parameters:
  x: 1.0
  y: 2.0
steps: |
  w = add(x, 2) ^ y
)";
        EXPECT_EQ(out, expected);

        grunk.write("test.grr.yml", recipe);
    }

    auto recipe = grunk.read("test.grr.yml");
    auto w = recipe.get_feature("w");
    EXPECT_NEAR(w.value().as<double>(), 9., 1e-10);
    auto y = recipe.get_feature("y");
    EXPECT_NEAR(y.value().as<double>(), 2., 1e-10);
    auto x = recipe.get_feature("x");
    EXPECT_NEAR(x.value().as<double>(), 1., 1e-10);
}

TEST(Recipe, no_parameters)
{
    grunk::state grunk;
    grunk.register_function("add", &add);
 
    {
        auto z = grunk.action("add", 1., 2.);

        auto recipe = grunk.create_recipe();
        recipe["w"] = grunk::pow(z, 2).with_id("w");

        std::string out = "\n" + recipe.to_string();
        std::string expected = R"(
uses:
  grunk: )" grunk_VERSION R"(
steps: |
  w = add(1.0, 2.0) ^ 2
)";
        EXPECT_EQ(out, expected);

        grunk.write("test.grr.yml", recipe);
    }

    auto recipe = grunk.read("test.grr.yml");
    auto w = recipe.get_feature("w");
    EXPECT_NEAR(w.value().as<double>(), 9., 1e-10);
}


namespace {

    class MyScalar
    {
    public:
        MyScalar(double v, std::string const& s) : m_value(v), m_tag(s) {}
        double value() const
        {
            return m_value;
        }

        std::string const& tag() const
        {
            return m_tag;
        }

        MyScalar pow(double exponent) {
            return {::pow(m_value, exponent), "POW!"};
        }

        void set(double v) {
            m_value = v;
        }
    private:
        double m_value;
        std::string m_tag;
    };

    MyScalar operator+(MyScalar const& l, MyScalar const& r) {
        return MyScalar(l.value() + r.value(), "add");
    }
}

TEST(Recipe, simple_userdata_parameters)
{
    grunk::state grunk;
    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors([](double v, std::string const& s){ return MyScalar(v,s); })
    .add_member_function("value", &MyScalar::value)
    .add_member_function("tag", &MyScalar::tag)
    .add_member_function("pow", &MyScalar::pow)
    .add_member_function("set", &MyScalar::set)
    .add_member_function("__add", [](MyScalar const& l, MyScalar const&r){
        return l + r;
    })
    .set(sol::meta_function::to_string, [](MyScalar const& s){
        return "MyScalar.new(" + grunk::to_string(s.value()) + ", \"" + s.tag() + "\")";
    });
 
    {
        auto a = grunk.feature("MyScalar", 31., "horst");
        auto b = grunk.feature("MyScalar", 11., "annette");
        auto c = a + b;
        auto d = grunk.action("MyScalar.pow", c, 2);

        auto recipe = grunk.create_recipe();
        recipe["a"] = a;
        recipe["b"] = b;
        recipe["c"] = c;
        recipe["d"] = d;
        recipe.tag();
        std::string out = "\n" + recipe.to_string();
        std::string expected = R"(
uses:
  grunk: )" grunk_VERSION R"(
parameters:
  a: MyScalar.new(31.0, "horst")
  b: MyScalar.new(11.0, "annette")
steps: |
  c = a + b
  d = MyScalar.pow(c, 2)
)";
        EXPECT_EQ(out, expected);

        grunk.write("test.grr.yml", recipe);
    }

    auto recipe = grunk.read("test.grr.yml");

    auto a = recipe.get_feature("a");
    EXPECT_NEAR(a.value().as<MyScalar>().value(), 31, 1e-10);
    auto b = recipe.get_feature("b");
    EXPECT_NEAR(b.value().as<MyScalar>().value(), 11, 1e-10);
    auto c = recipe.get_feature("c");
    EXPECT_NEAR(c.value().as<MyScalar>().value(), 42, 1e-10);
    auto d = recipe.get_feature("d");
    EXPECT_NEAR(d.value().as<MyScalar>().value(), 1764, 1e-10);
        
}

TEST(Recipe, serialize_with_subrecipes)
{
    grunk::state grunk;
    grunk.register_function("add", &add);

    {
        // create inner recipe
        auto recipe_inner = grunk.create_recipe();
        auto x = grunk.feature(17.).with_id("x");
        auto y = grunk.feature(11.).with_id("y");
        auto z = grunk.action("add", x, y).with_id("z");
        recipe_inner["x"] = x;
        recipe_inner["y"] = y;
        recipe_inner["z"] = z;

        // create outer recipe
        auto recipe = grunk.create_recipe();
        recipe["a"] = grunk.feature(13.).with_id("a");
        recipe["b"] = grunk.feature(11.).with_id("b");
        recipe.insert_recipe("addition", std::move(recipe_inner));

        std::string out = "\n" + recipe.to_string();
        std::string expected = R"(
uses:
  grunk: )" grunk_VERSION R"(
parameters:
  a: 13.0
  b: 11.0
recipes:
  addition:
    uses:
      grunk: )" grunk_VERSION R"(
    parameters:
      x: 17.0
      y: 11.0
    steps: |
      z = add(x, y)
)";
        EXPECT_EQ(out, expected);

        grunk.write("test.grr.yml", recipe);
    }

    auto recipe = grunk.read("test.grr.yml");

    EXPECT_EQ(recipe.get_feature("a").value().as<double>(), 13.);
    EXPECT_EQ(recipe.get_feature("b").value().as<double>(), 11.);

    auto& inner = recipe.get_recipe("addition").change_value();
    EXPECT_EQ(inner.get_feature("x").value().as<double>(), 17.);
    EXPECT_EQ(inner.get_feature("y").value().as<double>(), 11.);
    EXPECT_EQ(inner.get_feature("z").value().as<double>(), 28.);
}