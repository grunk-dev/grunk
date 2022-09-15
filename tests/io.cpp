#include <gtest/gtest.h>

#include <grunk/grunk.h>

using namespace grunk;

class IOTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        auto& plugins = grunk::get_plugin_registry();
        plugins.prepend_path(".");
        plugins.load_all();

        register_function(
            [](double const& l, double const& r){ return l+r;}, 
            "plus"
        );
    } 

};

TEST_F(IOTest, serialize_type)
{
    // test some specializations of 
    // parametric::serialize

    // int
    auto i = parametric::serialize(42);
    EXPECT_EQ(std::stoi(i), 42);

    // double
    auto d = parametric::serialize(0.33);
    EXPECT_NEAR(std::stof(d), 0.33, 1e-6);

    // string
    auto s = parametric::serialize(std::string("Hey Universe"));
    EXPECT_EQ(s, "Hey Universe");

    // DynamicObject
    auto x = Reflect::DynamicObject(1.23);
    auto sx = parametric::serialize(x);
    auto y = YAML::Load(sx);
    EXPECT_EQ(y.size(), 2);
    EXPECT_EQ(y["type"].as<std::string>(), "double");
    EXPECT_NEAR(y["value"].as<double>(), 1.23, 1e-6);

    // type without serialize specialization
    EXPECT_THROW(parametric::serialize([]{}), std::logic_error);
}

TEST_F(IOTest, serialize_node)
{
    // test overwrites of virtual DAGNode::serialize
    auto x = Feature("x", 0.2);
    auto y = Feature("y", 0.5);
    auto z = eval("z", "add", x, y);
    auto w = eval("w", [](auto const& x){ return x; }, y);

    // root parameters
    auto sx = x.param().node_pointer()->serialize();
    EXPECT_EQ(sx, parametric::serialize(0.2));

    auto sy = y.param().node_pointer()->serialize();
    EXPECT_EQ(sy, parametric::serialize(0.5));

    // compute node
    auto szc = z->serialize();
    auto yc = YAML::Load(szc);
    EXPECT_EQ(yc.size(), 3);
    EXPECT_EQ(yc["function"].as<std::string>(), "add");
    EXPECT_EQ(yc["inputs"].size(), 2);
    EXPECT_EQ(yc["inputs"][0].as<std::string>(), "x");
    EXPECT_EQ(yc["inputs"][1].as<std::string>(), "y");
    EXPECT_EQ(yc["outputs"].size(), 1);
    EXPECT_EQ(yc["outputs"][0].as<std::string>(), "z");

    // Can't serialize algorithm with non registered function
    EXPECT_THROW(w->serialize(), std::logic_error);

    // dependent parameter
    auto zn = z->get().param().node_pointer()->serialize();
    EXPECT_EQ(zn, "");
}

TEST_F(IOTest, parse_feature_tree_empty)
{
    auto x = details::parse_feature_tree();

    // there should be just one node called "uses"
    EXPECT_EQ(x.size(), 1);
    EXPECT_EQ(x.begin()->first.as<std::string>(), "uses");
    auto uses = x["uses"];
    
    // in "uses", there should be two nodes called "grunk" and "SimplePlugin"
    EXPECT_EQ(uses.size(), 2);
    auto it = uses.begin();
    EXPECT_EQ(it->first.as<std::string>(), "grunk");
    it++;
    EXPECT_EQ(it->first.as<std::string>(), "SimplePlugin");

    EXPECT_EQ(uses["grunk"].as<std::string>(), grunk_VERSION);
    EXPECT_EQ(uses["SimplePlugin"].as<std::string>(), "1.2.3");
}

TEST_F(IOTest, basic)
{
    auto a = Feature("a", "double", 0.2);
    auto b = Feature("b", "double", 0.1);
    auto c = eval("c", "plus", a, b)->get();
    auto d = eval("d", "plus", c, a)->get();

    auto y = details::parse_feature_tree(d);
    EXPECT_EQ(y.size(), 3);
    EXPECT_EQ(y["parameters"].size(), 2);
    EXPECT_EQ(y["parameters"]["a"].size(), 2);
    EXPECT_EQ(y["parameters"]["a"]["type"].as<std::string>(), "double");
    EXPECT_NEAR(y["parameters"]["a"]["value"].as<double>(), 0.2, 1e-6);
    EXPECT_EQ(y["parameters"]["b"].size(), 2);
    EXPECT_EQ(y["parameters"]["b"]["type"].as<std::string>(), "double");
    EXPECT_NEAR(y["parameters"]["b"]["value"].as<double>(), 0.1, 1e-6);
    EXPECT_EQ(y["steps"].size(), 2);
    EXPECT_EQ(y["steps"][0].size(), 3);
    EXPECT_EQ(y["steps"][0]["function"].as<std::string>(), "plus");
    EXPECT_EQ(y["steps"][0]["inputs"].size(), 2);
    EXPECT_EQ(y["steps"][0]["inputs"][0].as<std::string>(), "a");
    EXPECT_EQ(y["steps"][0]["inputs"][1].as<std::string>(), "b");
    EXPECT_EQ(y["steps"][0]["outputs"].size(), 1);
    EXPECT_EQ(y["steps"][0]["outputs"][0].as<std::string>(), "c");
    EXPECT_EQ(y["steps"][1].size(), 3);
    EXPECT_EQ(y["steps"][1]["function"].as<std::string>(), "plus");
    EXPECT_EQ(y["steps"][1]["inputs"].size(), 2);
    EXPECT_EQ(y["steps"][1]["inputs"][0].as<std::string>(), "c");
    EXPECT_EQ(y["steps"][1]["inputs"][1].as<std::string>(), "a");
    EXPECT_EQ(y["steps"][1]["outputs"].size(), 1);
    EXPECT_EQ(y["steps"][1]["outputs"][0].as<std::string>(), "d");

    // adding nodes that d depends on shouldn't 
    // alter the output
    // NOTE: Not true for the order of root parameters
    auto str = to_string(d);
    EXPECT_EQ(str, to_string(d,a));
    EXPECT_EQ(str, to_string(a,d,b));
    EXPECT_EQ(str, to_string(d,b));
    EXPECT_EQ(str, to_string(a,a,d,a));
}

TEST_F(IOTest, simple_plugin)
{
    auto a = Feature("a", "MyDouble", 0.2);
    auto b = Feature("b", "MyDouble", 0.1);
    auto c = eval("c", "add", a, b)->get();
    auto d = eval("d", "add", c, a)->get();

    auto y = details::parse_feature_tree(d);
    EXPECT_EQ(y.size(), 3);
    EXPECT_EQ(y["parameters"].size(), 2);
    EXPECT_EQ(y["parameters"]["a"].size(), 2);
    EXPECT_EQ(y["parameters"]["a"]["type"].as<std::string>(), "MyDouble");
    EXPECT_NEAR(y["parameters"]["a"]["value"].as<double>(), 0.2, 1e-6);
    EXPECT_EQ(y["parameters"]["b"].size(), 2);
    EXPECT_EQ(y["parameters"]["b"]["type"].as<std::string>(), "MyDouble");
    EXPECT_NEAR(y["parameters"]["b"]["value"].as<double>(), 0.1, 1e-6);
    EXPECT_EQ(y["steps"].size(), 2);
    EXPECT_EQ(y["steps"][0].size(), 3);
    EXPECT_EQ(y["steps"][0]["function"].as<std::string>(), "add");
    EXPECT_EQ(y["steps"][0]["inputs"].size(), 2);
    EXPECT_EQ(y["steps"][0]["inputs"][0].as<std::string>(), "a");
    EXPECT_EQ(y["steps"][0]["inputs"][1].as<std::string>(), "b");
    EXPECT_EQ(y["steps"][0]["outputs"].size(), 1);
    EXPECT_EQ(y["steps"][0]["outputs"][0].as<std::string>(), "c");
    EXPECT_EQ(y["steps"][1].size(), 3);
    EXPECT_EQ(y["steps"][1]["function"].as<std::string>(), "add");
    EXPECT_EQ(y["steps"][1]["inputs"].size(), 2);
    EXPECT_EQ(y["steps"][1]["inputs"][0].as<std::string>(), "c");
    EXPECT_EQ(y["steps"][1]["inputs"][1].as<std::string>(), "a");
    EXPECT_EQ(y["steps"][1]["outputs"].size(), 1);
    EXPECT_EQ(y["steps"][1]["outputs"][0].as<std::string>(), "d");
}

TEST_F(IOTest, write)
{
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("b", "double", 0.1);
        auto c = eval("c", "plus", a, b)->get();
        auto d = eval("d", "plus", c, a)->get();
        write("test.gk", d);
    }

    YAML::LoadFile("test.gk");
}

// To Do:
//  - test writing to file: results by making a reusable functions from the test before
//  - test writing given any const iterable constainer of RuntimeFeatures
//  - test reading from file 
//  - test deserializing a type
//  - test deserialize error on nonexistent function
//  - test deserialize error on nonexistent type
//  - test roundtrip starting from tree
//  - test roundtrip starting from file