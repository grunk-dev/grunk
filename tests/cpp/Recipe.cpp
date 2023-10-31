#include <gtest/gtest.h>

#include <grunk/grunk.hpp>

using namespace grunk;

class RecipeTest : public ::testing::Test
{
public:
    static void SetUpTestCase() {

        grunk::StdPlugin().init();

        reflect::register_function(
            [](double x, double y){ return x+y; }, 
            "add"
        );
    }

    static void TearDownTestCase() {
        reflect::get_type_registry().clear();
    } 
};

TEST_F(RecipeTest, ctor)
{
    EXPECT_NO_THROW(Recipe recipe);

    auto x = Feature("x", "double", 0.2);
    auto y = Feature("y", "double", 0.1);
    EXPECT_NO_THROW(Recipe recipe(x));
    EXPECT_NO_THROW(Recipe recipe(x, y));
    EXPECT_NO_THROW(Recipe recipe({x, y}));

    grunk::Recipe::FeatureContainer m{{"x", x}, {"y", y}};
    EXPECT_NO_THROW(Recipe recipe(m));
    
}

TEST_F(RecipeTest, clone)
{
    Feature x("x", "double", 12.3);
    Feature y("y", "double", 29.7);
    Feature z = action("z", "add", x, y).output();
    ASSERT_NEAR(z.value().as<double>(), 42., 1e-15);

    Recipe my_recipe({x, y, z});
    
    Recipe clone = my_recipe.clone();

    EXPECT_EQ(clone.num_features(), 3);
    EXPECT_EQ(clone.num_recipes(), 0);

    EXPECT_TRUE(clone.at("x").get_type_descriptor());
    
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

TEST_F(RecipeTest, clone_with_subrecipes)
{
    // create inner recipe
    Feature x("x", "double", 17.);
    Feature y("y", "double", 11.);
    Feature z = action("z", "add", x, y).output();
    Recipe recipe1({x, y, z});

    // create outer recipe
    Feature a("a", "double", 13.);
    Feature b("b", "double", 11.);
    Recipe recipe({a, b});
    recipe.insert_recipe("addition", std::move(recipe1));

    // evaluate inner recipe
    recipe.recipe(
        "addition",
        {{"c", "z"}}, 
        {{"x", a}, {"y", b}}
    );

    // just some quick sanity checks
    auto other = recipe.clone();
    EXPECT_EQ(other.num_features(), 3);
    EXPECT_EQ(other.num_recipes(), 1);
    EXPECT_EQ(other.get_recipe("addition").num_features(), 3);
}

TEST_F(RecipeTest, as_function)
{
    Feature x("x", "double", 12.3);
    Feature y("y", "double", 29.7);
    Feature z = action("z", "add", x, y).output();
    Recipe my_recipe({x, y, z});

    Feature a("a", "double", -10.);
    auto res = my_recipe(
        "my_recipe",
        {{"my_z", "z"}}, // create a new node my_z that contains the value of the inner node z
        {{"x", a}} // map the inner node x to input parameter a
    );
    Feature my_z = res.at("my_z");
    EXPECT_NEAR(my_z.value().as<double>(), 19.7, 1e-15);
    EXPECT_EQ(my_z.id(), "my_z");

    // my _recipe should be unaffected by call to Recipe::operator
    EXPECT_EQ(my_recipe.at("x").value().as<double>(), 12.3);
    EXPECT_NEAR(my_recipe.at("z").value().as<double>(), 42., 1e-15);
}

TEST_F(RecipeTest, deserialize_no_uses_block)
{
    YAML::Node root;
    EXPECT_THROW(
        Recipe::deserialize(root),
        grunk::io_error
    );
}

TEST_F(RecipeTest, deserialize_non_existing_function)
{
    YAML::Node root;
    
    YAML::Node uses;
    uses["grunk"] = grunk_VERSION;
    root["uses"] = uses;

    YAML::Node parameters;
    parameters["a"] = YAML::Load(parametric::serialize(reflect::DynamicObject(13.2)));
    parameters["b"] = YAML::Load(parametric::serialize(reflect::DynamicObject(11.8)));
    root["parameters"] = parameters;

    YAML::Node steps;

    YAML::Node step = YAML::Node();
    step.SetTag("spunck");
    step.push_back(std::vector<std::string>{"c"});
    step.push_back(std::vector<std::string>{"a", "b"});
    steps.push_back(step);
    root["steps"] = steps;

    EXPECT_THROW(
        Recipe::deserialize(root),
        reflect::Unresolvable
    );
}

TEST_F(RecipeTest, deserialize_no_topo_order)
{
    YAML::Node root;
    
    YAML::Node uses;
    uses["grunk"] = grunk_VERSION;
    root["uses"] = uses;

    YAML::Node parameters;
    parameters["a"] = YAML::Load(parametric::serialize(reflect::DynamicObject(13.2)));
    parameters["b"] = YAML::Load(parametric::serialize(reflect::DynamicObject(11.8)));
    root["parameters"] = parameters;

    YAML::Node steps;

    YAML::Node step2;
    step2.SetTag("add");
    step2.push_back(std::vector<std::string>{"d"});
    step2.push_back(std::vector<std::string>{"a", "c"});
    steps.push_back(step2);

    YAML::Node step1;
    step1.SetTag("add");
    step1.push_back(std::vector<std::string>{"c"});
    step1.push_back(std::vector<std::string>{"a", "b"});
    steps.push_back(step1);

    root["steps"] = steps;

    EXPECT_THROW(
        Recipe::deserialize(root), 
        grunk::io_error
    );
}

TEST_F(RecipeTest, deserialize_wrong_value)
{
    YAML::Node root;


    YAML::Node uses;
    uses["grunk"] = grunk_VERSION;
    root["uses"] = uses;

    YAML::Node parameters;
    auto a = YAML::Node("Hello World");
    a.SetTag("double");
    parameters["a"] = a;
    root["parameters"] = parameters;

    EXPECT_THROW(
        Recipe::deserialize(root), 
        grunk::io_error
    );
}

TEST_F(RecipeTest, serialize_no_subrecipe)
{
    YAML::Node node;

    {
        Recipe recipe;
        recipe.insert_feature(Feature("x", "double", -0.15));
        recipe.insert_feature(Feature("y", "double", -0.75));
        auto add_node = action("z", "add", recipe.at("x"), recipe.at("y"));
        recipe.insert_feature(add_node.output());

        node = recipe.serialize();
    } // desctructor of recipe called

    EXPECT_EQ(node.size(), 3);
    EXPECT_EQ(node["parameters"].size(), 2);
    EXPECT_EQ(node["parameters"]["x"].Tag(), "double");
    EXPECT_NEAR(node["parameters"]["x"].as<double>(), -0.15, 1e-6);
    EXPECT_EQ(node["parameters"]["y"].Tag(), "double");
    EXPECT_NEAR(node["parameters"]["y"].as<double>(), -0.75, 1e-6);

    EXPECT_EQ(node["steps"].size(), 1);
    auto step1 = node["steps"][0];
    EXPECT_EQ(step1.Tag(), "add");
    EXPECT_EQ(step1[0].size(), 1);
    EXPECT_EQ(step1[0][0].as<std::string>(), "z");
    EXPECT_EQ(step1[1].size(), 2);
    EXPECT_EQ(step1[1][0].as<std::string>(), "x");
    EXPECT_EQ(step1[1][1].as<std::string>(), "y");

    Recipe deserialized = Recipe::deserialize(node);
    EXPECT_NEAR(deserialized.at("x").value().as<double>(), -0.15, 1e-6);
    EXPECT_NEAR(deserialized.at("y").value().as<double>(), -0.75, 1e-6);
    EXPECT_NEAR(deserialized.at("z").value().as<double>(), -0.9, 1e-6);
}

TEST_F(RecipeTest, serialize_subrecipe)
{
    YAML::Node node;

    {
        Feature x("x", "double", 12.3);
        Feature y("y", "double", 29.7);
        Feature z = action("z", "add", x, y).output();
        Recipe recipe1({x, y, z});

        auto recipe2 = std::make_unique<Recipe>();
        recipe2->insert_feature(Feature("a", "double", 1.));
        recipe2->insert_feature(Feature("b", "double", 2.));
        auto add_node = action("c", "add", recipe2->at("a"), recipe2->at("b"));
        recipe2->insert_feature(add_node.output());

        recipe1.insert_recipe("recipe2", std::move(recipe2));

        node = recipe1.serialize();
    }

    EXPECT_EQ(node.size(), 4);
    EXPECT_TRUE(node["recipes"]);
    EXPECT_TRUE(node["recipes"]["recipe2"]);

    auto recipe2_node = node["recipes"]["recipe2"];
    EXPECT_EQ(recipe2_node["parameters"].size(), 2);
    EXPECT_EQ(recipe2_node["parameters"]["a"].Tag(), "double");
    EXPECT_NEAR(recipe2_node["parameters"]["a"].as<double>(), 1., 1e-6);
    EXPECT_EQ(recipe2_node["parameters"]["b"].Tag(), "double");
    EXPECT_NEAR(recipe2_node["parameters"]["b"].as<double>(), 2., 1e-6);

    EXPECT_EQ(recipe2_node["steps"].size(), 1);
    auto step1 = node["steps"][0];
    EXPECT_EQ(step1.Tag(), "add");
    EXPECT_EQ(step1[0].size(), 1);
    EXPECT_EQ(step1[0][0].as<std::string>(), "z");
    EXPECT_EQ(step1[1].size(), 2);
    EXPECT_EQ(step1[1][0].as<std::string>(), "x");
    EXPECT_EQ(step1[1][1].as<std::string>(), "y");
}

TEST_F(RecipeTest, serialize_recipe_action)
{
    // create inner recipe
    Feature x("x", "double", 17.);
    Feature y("y", "double", 11.);
    Feature z = action("z", "add", x, y).output();
    Recipe recipe1({x, y, z});

    // create outer recipe
    Feature a("a", "double", 13.);
    Feature b("b", "double", 11.);
    Recipe recipe({a, b});
    recipe.insert_recipe("addition", std::move(recipe1));

    // evaluate inner recipe
    recipe.recipe(
        "addition",
        {{"c", "z"}}, 
        {{"x", a}, {"y", b}}
    );

    EXPECT_NEAR(recipe.at("c").value().as<double>(), 24., 1e-15);
    
    auto node = recipe.serialize();
    EXPECT_EQ(node.size(), 4);
    EXPECT_EQ(node["steps"].size(), 1);
    auto step = node["steps"][0];

    // check tag
    EXPECT_EQ(step.Tag(), "recipes::addition");

    // check output map
    EXPECT_EQ(step[0].Type(), YAML::NodeType::Map);
    EXPECT_EQ(step[0].size(), 1);
    EXPECT_TRUE(step[0]["c"]);
    EXPECT_EQ(step[0]["c"].as<std::string>(), "z");

    // check input map
    EXPECT_EQ(step[1].Type(), YAML::NodeType::Map);
    EXPECT_EQ(step[1].size(), 2);
    EXPECT_TRUE(step[1]["x"]);
    EXPECT_EQ(step[1]["x"].as<std::string>(), "a");
    EXPECT_TRUE(step[1]["y"]);
    EXPECT_EQ(step[1]["y"].as<std::string>(), "b");
}


TEST_F(RecipeTest, deserialize_recipe_action)
{
    YAML::Node serialized;
    
    {
        // create inner recipe
        Recipe recipe1;
        recipe1.feature("x", "double", 17.);
        recipe1.feature("y", "double", 5.);
        recipe1.insert_feature(
            action(
                "z", 
                "add", 
                recipe1.at("x"), 
                recipe1.at("y")
            ).output()
        );
        
        // create outer recipe
        Feature a("a", "double", 13.);
        Feature b("b", "double", 11.);
        Recipe recipe({a, b});
        recipe.insert_recipe("addition", std::move(recipe1));

        // evaluate inner recipe
        recipe.recipe(
            "addition",
            {{"c", "z"}}, 
            {{"x", a}, {"y", b}}
        );

        serialized = recipe.serialize();
    }
    
    auto recipe = Recipe::deserialize(serialized);
    EXPECT_EQ(recipe.num_features(), 3);
    EXPECT_EQ(recipe.at("a").value().as<double>(), 13.);
    EXPECT_EQ(recipe.at("b").value().as<double>(), 11.);
    EXPECT_EQ(recipe.at("c").value().as<double>(), 24.);
    EXPECT_EQ(recipe.num_recipes(), 1);
    auto& subrecipe = recipe.get_recipe("addition");
    EXPECT_EQ(subrecipe.num_features(), 3);
    EXPECT_EQ(subrecipe.at("x").value().as<double>(), 17.);
    EXPECT_EQ(subrecipe.at("y").value().as<double>(), 5.);
    EXPECT_EQ(subrecipe.at("z").value().as<double>(), 22.);
}

TEST_F(RecipeTest, constants)
{
    {
        Feature x("x", "double", 12.3);
        Feature y("y", "double", 29.7);
        Feature z = action("z", "add", x, y).output();
        Recipe my_recipe({x, y, z});

        Feature a("", "double", -10.);
        auto res = my_recipe(
            "my_recipe",
            {{"my_z", "z"}}, // create a new node my_z that contains the value of the inner node z
            {{"x", a}} // map the inner node x to input parameter a
        );
        Feature my_z = res.at("my_z");
        
        YAML::Node n = grunk::serialize(my_z);
        EXPECT_FALSE(n["parameters"]);
        EXPECT_EQ(n["steps"].size(), 1);
        EXPECT_EQ(n["recipes"].size(), 1);
        EXPECT_EQ(n["steps"][0][1]["x"].Tag(), "double");

        grunk::write("tmp.grr.yml", my_z);
    }

    auto r = grunk::read("tmp.grr.yml");
    EXPECT_EQ(r.num_features(), 1);
    EXPECT_EQ(r.num_recipes(), 1);
    EXPECT_NEAR(r["my_z"].value().as<double>(), 19.7, 1e-15);
}