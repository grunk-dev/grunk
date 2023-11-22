#include <gtest/gtest.h>

#include <grunk/grunk.hpp>
#include <stdexcept>

using namespace grunk;

namespace {

struct NonSerializable
{
    // needs non-default ctor (for now, until 
    // https://gitlab.dlr.de/paradigms/grunk/-/issues/39 gets fixed)
    NonSerializable(int v) : value(v) {}
    int value;
};

}

class IOTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        auto& plugins = grunk::get_plugin_registry();
        plugins.prepend_path(".");
        plugins.load_all();

        reflect::register_type<NonSerializable>("NonSerializable")
        .add_constructor<int>();

        reflect::register_function(
            [](double const& l, double const& r){ return l+r;}, 
            "plus"
        );

        reflect::register_function(
            [](double x){ return x*x; },
            "squared"
        );
    } 

};

TEST_F(IOTest, no_serialize_method)
{
    auto x = Feature("x", "NonSerializable", 42);
    EXPECT_THROW(x.param().node_pointer()->serialize(), std::out_of_range);
}

TEST_F(IOTest, serialize_type)
{
    // test some specializations of 
    // parametric::serialize

    // int
    auto i = parametric::serialize(42);
    auto ni = YAML::Load(i);
    EXPECT_EQ(ni.as<int>(), 42);
    EXPECT_EQ(ni.Tag(), "int");

    // double
    auto d = parametric::serialize(0.33);
    auto nd = YAML::Load(d);
    EXPECT_NEAR(nd.as<double>(), 0.33, 1e-6);
    EXPECT_EQ(nd.Tag(), "double");

    // string
    auto s = parametric::serialize(std::string("Hey Universe"));
    EXPECT_EQ(s, "!<String> Hey Universe");

    // DynamicObject
    auto x = reflect::DynamicObject(1.23);
    auto sx = parametric::serialize(x);
    auto y = YAML::Load(sx);

    EXPECT_EQ(y.Tag(), "double");
    EXPECT_NEAR(y.as<double>(), 1.23, 1e-6);

    // type without serialize specialization
    EXPECT_THROW(parametric::serialize([]{}), std::logic_error);
}

TEST_F(IOTest, serialize_DAGNode)
{
    // test overwrites of virtual DAGNode::serialize
    auto x = Feature("x", 0.2);
    auto y = Feature("y", "double", 0.5);
    auto z = action("z", "SimplePlugin::add", x, y);
    auto w = action("w", [](auto const& x){ return x; }, y);

    // root parameters
    auto sx = x.param().node_pointer()->serialize();
    EXPECT_EQ(sx, parametric::serialize(0.2));

    auto sy = y.param().node_pointer()->serialize();
    EXPECT_EQ(sy, parametric::serialize(reflect::make_dynamic("double", 0.5)));

    // compute node
    auto szc = z.compute_node()->serialize();
    auto yc = YAML::Load(szc); 
    EXPECT_EQ(yc.Tag(), "SimplePlugin::add");

    //two arrays, one for outputs one for inputs
    EXPECT_EQ(yc.size(), 2);

    //outputs
    EXPECT_EQ(yc[0].size(), 1);
    EXPECT_EQ(yc[0][0].as<std::string>(), "z");

    //inputs
    EXPECT_EQ(yc[1].size(), 2);
    EXPECT_EQ(yc[1][0].as<std::string>(), "x");
    EXPECT_EQ(yc[1][1].as<std::string>(), "y");

    // Can't serialize action with non registered function
    EXPECT_THROW(w.compute_node()->serialize(), std::logic_error);

    // dependent parameter
    auto zo = z;
    EXPECT_EQ(
        parametric::serialize(zo.output().value()),      // call specialization directly
        zo.output().param().node_pointer()->serialize()  // call via DAGNode::serialize member function
    );
}

TEST_F(IOTest, recipe_to_yaml_empty)
{
    auto x = grunk::serialize();

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

/**
 * @brief use this utility function to test serialization of a 
 * tree with topology and node names
 *
 *  c = func(a,b); d=func(c,a)
 *
 * where the function name and type name of a and b may differ. 
 * The nodes a and b must serialize to a double (could be e.g.
 * double, or MyDouble from SimplePlugin)
 * 
 * @param node 
 * @param function_name 
 */
void test_basic_tree(
    YAML::Node const& node, 
    std::string const& function_name, 
    std::string const& type_name
)
{
    EXPECT_EQ(node.size(), 3);
    EXPECT_EQ(node["parameters"].size(), 2);
    EXPECT_EQ(node["parameters"]["a"].Tag(), type_name);
    EXPECT_NEAR(node["parameters"]["a"].as<double>(), 0.2, 1e-6);
    EXPECT_EQ(node["parameters"]["b"].Tag(), type_name);
    EXPECT_NEAR(node["parameters"]["b"].as<double>(), 0.1, 1e-6);
    
    EXPECT_EQ(node["steps"].size(), 2);
 
    auto step1 = node["steps"][0];
    EXPECT_EQ(step1.Tag(), function_name);
    EXPECT_EQ(step1[0].size(), 1);
    EXPECT_EQ(step1[0][0].as<std::string>(), "c");
    EXPECT_EQ(step1[1].size(), 2);
    EXPECT_EQ(step1[1][0].as<std::string>(), "a");
    EXPECT_EQ(step1[1][1].as<std::string>(), "b");

    auto step2 = node["steps"][1];
    EXPECT_EQ(step2.Tag(), function_name);
    EXPECT_EQ(step2[0].size(), 1);
    EXPECT_EQ(step2[0][0].as<std::string>(), "d");
    EXPECT_EQ(step2[1].size(), 2);
    EXPECT_EQ(step2[1][0].as<std::string>(), "c");
    EXPECT_EQ(step2[1][1].as<std::string>(), "a");
}

TEST_F(IOTest, basic)
{
    auto a = Feature("a", "double", 0.2);
    auto b = Feature("b", "double", 0.1);
    auto c = action("c", "plus", a, b).output();
    auto d = action("d", "plus", c, a).output();

    auto y = grunk::serialize(d);
    test_basic_tree(y, "plus", "double");

    // adding nodes that d depends on shouldn't 
    // alter the output
    // NOTE: Not true for the order of root parameters
    test_basic_tree(serialize(d,a), "plus", "double");
    test_basic_tree(serialize(a,d,b), "plus", "double");
    test_basic_tree(serialize(d,b), "plus", "double");
    test_basic_tree(serialize(a,d), "plus", "double");
    test_basic_tree(serialize(a,b,d), "plus", "double");
    test_basic_tree(serialize(b,d), "plus", "double");
    test_basic_tree(serialize(a,d), "plus", "double");
}

TEST_F(IOTest, simple_plugin)
{
    auto a = Feature("a", "SimplePlugin::MyDouble", 0.2);
    auto b = Feature("b", "SimplePlugin::MyDouble", 0.1);
    auto c = action("c", "SimplePlugin::add", a, b).output();
    auto d = action("d", "SimplePlugin::add", c, a).output();

    auto y = serialize(d);
    test_basic_tree(y, "SimplePlugin::add", "SimplePlugin::MyDouble");
}

TEST_F(IOTest, write)
{
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("b", "double", 0.1);
        auto c = action("c", "plus", a, b).output();
        auto d = action("d", "plus", c, a).output();
        write("test.grr", d);
    }

    auto y = YAML::LoadFile("test.grr");
    test_basic_tree(y, "plus", "double");
}

TEST_F(IOTest, deserialize_double)
{
    auto y = YAML::Node(0.9876);
    y.SetTag("double");

    auto d = details::deserialize(
        y.Tag(),
        y
    );
    EXPECT_NEAR(reflect::cast<double>(d), 0.9876, 1e-7);
}

TEST_F(IOTest, deserialize_simple_plugin_MyDouble)
{
    auto y = YAML::Node(0.55557);
    y.SetTag("SimplePlugin::MyDouble");

    auto md = details::deserialize(
        y.Tag(),
        y
    );
    auto d = md.get("value");
    EXPECT_NEAR(reflect::cast<double>(d), 0.55557, 1e-7);
}

TEST_F(IOTest, no_deserialize_method)
{
    auto y = YAML::Node("doesnt matter what I write here");
    y.SetTag("NonSerializable");

    EXPECT_THROW(
        details::deserialize(
            y.Tag(),
            y
        ),
        grunk::io_error
    );
}

TEST_F(IOTest, deserialize_non_existing_type)
{
    auto y= YAML::Node(0.33);
    y.SetTag("NonExistentType");

    EXPECT_THROW(
        details::deserialize(
            y.Tag(),
            y
        ),
        grunk::io_error
    );
}

TEST_F(IOTest, write_duplicate_name)
{
    // duplicate name in parameters
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("a", "double", 0.1);

        EXPECT_THROW(
            serialize(a, b),
            std::logic_error
        );
    }

    // duplicate name in steps
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("b", "double", 0.1);
        auto c = action("a", "plus", a, b).output();
        auto d = action("d", "plus", b, a).output();

        EXPECT_THROW(
            serialize(d, c),
            grunk::io_error
        );
    }
}

TEST_F(IOTest, read_duplicate_name)
{

    EXPECT_THROW(
        read("test_data/simple_test_duplicate_name_parameter.grr"),
        grunk::io_error
    );

    EXPECT_THROW(
        read("test_data/simple_test_duplicate_name_step.grr"),
        grunk::io_error
    );
}

TEST_F(IOTest, roundtrip_write_read)
{
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("b", "double", 0.1);
        auto c = action("c", "plus", a, b).output();
        auto d = action("d", "plus", c, a).output();
        write("test.grr", d);
    }

    {
        auto recipe = read("test.grr");
        EXPECT_EQ(recipe.num_features(), 4);
        EXPECT_NEAR(reflect::cast<double>(recipe.at("d").value()), 0.5, 1e-7);
        EXPECT_NEAR(reflect::cast<double>(recipe.at("c").value()), 0.3, 1e-7);
        EXPECT_NEAR(reflect::cast<double>(recipe.at("b").value()), 0.1, 1e-7);
        EXPECT_NEAR(reflect::cast<double>(recipe.at("a").value()), 0.2, 1e-7);
    }
}

TEST_F(IOTest, roundtrip_read_write)
{
    auto recipe = read("test_data/simple_test.grr");

    EXPECT_EQ(recipe.num_features(), 4);
    EXPECT_NEAR(reflect::cast<double>(recipe.at("d").value().get("value")), 0.5, 1e-7);
    EXPECT_NEAR(reflect::cast<double>(recipe.at("c").value().get("value")), 0.3, 1e-7);
    EXPECT_NEAR(reflect::cast<double>(recipe.at("b").value().get("value")), 0.1, 1e-7);
    EXPECT_NEAR(reflect::cast<double>(recipe.at("a").value().get("value")), 0.2, 1e-7);

    auto y = serialize(recipe.at("d"));
    test_basic_tree(y, "SimplePlugin::add", "SimplePlugin::MyDouble");
}

TEST_F(IOTest, fully_qualified_name)
{
    auto a = Feature("a", "SimplePlugin::MyDouble", 0.2);
    auto b = action("b", "SimplePlugin::MyDouble::half", a).output();

    EXPECT_NEAR(b.value().get_as<double>("value"), 0.1, 1e-15);
    auto y = serialize(b);

    EXPECT_EQ(y["steps"].size(), 1);
 
    auto step1 = y["steps"][0];
    EXPECT_EQ(step1.Tag(), "SimplePlugin::MyDouble::half");
}

TEST_F(IOTest, topo_order){
    // https://gitlab.dlr.de/paradigms/grunk/-/issues/91
    auto a= Feature("a", "double", 2);
    auto b = action("b", "squared", a).output();
    auto c = action("c", "squared", b).output();
    auto d = action("d", "squared", b).output();
    auto e = action("e", "plus", c, d).output();
    auto x = Feature("x", "double", 4);
    auto z = action("z", "plus", e, x).output();


    YAML::Node root;
    details::ToStringVisitor visitor(root);

    details::parse_feature(x, visitor);
    details::parse_feature(z, visitor);

    visitor.unwind_steps();

    std::unordered_map<std::string, bool> nodes;
    // add parameters to the "parsed" nodes manually
    nodes["a"] = true;
    nodes["x"] = true;
    for (auto const& s : root["steps"]) {
        for (auto const& input : s[1]) {
            // make sure all inputs have already been added to the nodes
            ASSERT_TRUE(nodes[input.as<std::string>()]);
        }
        for (auto const& output : s[0]) {
            nodes[output.as<std::string>()] = true;
        }
    }
}

TEST_F(IOTest, constants)
{
    YAML::Node s;
    {
        auto a = action("a", "squared", 2.).output();
        s = grunk::serialize(a);
        std::cout << grunk::to_string(a);
    }
    EXPECT_FALSE(s["parameters"]);
    EXPECT_EQ(s["steps"].size(), 1);
    auto step = s["steps"][0];
    EXPECT_EQ(step.size(), 2);
    EXPECT_EQ(step.Tag(), "squared");
    EXPECT_EQ(step[0].size(), 1);
    EXPECT_EQ(step[0][0].as<std::string>(), "a");
    EXPECT_EQ(step[1].size(), 1);
    EXPECT_EQ(step[1][0].Tag(), "double");
    EXPECT_NEAR(step[1][0].as<double>(), 2., 1e-10);

    auto recipe = grunk::Recipe::deserialize(s);
    EXPECT_EQ(recipe.get_features().size(), 1);
    EXPECT_NEAR(recipe["a"].value().as<double>(), 4, 1e-10);
}