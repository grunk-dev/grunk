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

        register_type<NonSerializable>("NonSerializable")
        .add_constructor<int>();

        register_function(
            [](double const& l, double const& r){ return l+r;}, 
            "plus"
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
    EXPECT_EQ(std::stoi(i), 42);

    // double
    auto d = parametric::serialize(0.33);
    EXPECT_NEAR(std::stof(d), 0.33, 1e-6);

    // string
    auto s = parametric::serialize(std::string("Hey Universe"));
    EXPECT_EQ(s, "Hey Universe");

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
    EXPECT_EQ(yc.Tag(), "add");

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
    EXPECT_THROW(w->serialize(), std::logic_error);

    // dependent parameter
    auto zn = z->get().param().node_pointer()->serialize();
    EXPECT_EQ(zn, "");
}

TEST_F(IOTest, feature_tree_to_yaml_empty)
{
    auto x = details::feature_tree_to_yaml<std::vector<grunk::DynamicFeature>>({});

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
    auto c = eval("c", "plus", a, b)->get();
    auto d = eval("d", "plus", c, a)->get();

    auto y = details::feature_tree_to_yaml(d);
    test_basic_tree(y, "plus", "double");

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

    auto y = details::feature_tree_to_yaml(d);
    test_basic_tree(y, "add", "MyDouble");
}

TEST_F(IOTest, write)
{
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("b", "double", 0.1);
        auto c = eval("c", "plus", a, b)->get();
        auto d = eval("d", "plus", c, a)->get();
        write("test.grr", d);
    }

    auto y = YAML::LoadFile("test.grr");
    test_basic_tree(y, "plus", "double");
}

TEST_F(IOTest, write_const_iterable_container)
{
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("b", "double", 0.1);
        auto c = eval("c", "plus", a, b)->get();
        auto d = eval("d", "plus", c, a)->get();

        std::vector<DynamicFeature> v{a,b,c,d};
        write("test_vector.grr", v);

        FeatureContainer m;
        m.insert({"a", a});
        m.insert({"b", b});
        m.insert({"c", c});
        m.insert({"d", d});
        write("test_unordered_map.grr", m);
    }

    auto yv = YAML::LoadFile("test_vector.grr");
    test_basic_tree(yv, "plus", "double");

    auto ym = YAML::LoadFile("test_unordered_map.grr");
    test_basic_tree(ym, "plus", "double");
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
    y.SetTag("MyDouble");

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

TEST_F(IOTest, deserialize_no_uses_block)
{
    YAML::Node root;
    EXPECT_THROW(
        details::yaml_to_feature_tree(root),
        grunk::io_error
    );
}

TEST_F(IOTest, deserialize_wrong_value)
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
        details::yaml_to_feature_tree(root), 
        grunk::io_error
    );
}

TEST_F(IOTest, deserialize_non_existing_function)
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
        details::yaml_to_feature_tree(root),
        std::out_of_range
    );
}

TEST_F(IOTest, deserialize_no_topo_order)
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
        details::yaml_to_feature_tree(root), 
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
            details::feature_tree_to_yaml(a, b),
            grunk::io_error
        );
    }

    // duplicate name in steps
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("b", "double", 0.1);
        auto c = eval("a", "plus", a, b)->get();
        auto d = eval("d", "plus", b, a)->get();

        EXPECT_THROW(
            details::feature_tree_to_yaml(d, c),
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
        auto c = eval("c", "plus", a, b)->get();
        auto d = eval("d", "plus", c, a)->get();
        write("test.grr", d);
    }

    {
        auto features = read("test.grr");
        EXPECT_EQ(features.size(), 4);
        EXPECT_NEAR(reflect::cast<double>(features.at("d").value()), 0.5, 1e-7);
        EXPECT_NEAR(reflect::cast<double>(features.at("c").value()), 0.3, 1e-7);
        EXPECT_NEAR(reflect::cast<double>(features.at("b").value()), 0.1, 1e-7);
        EXPECT_NEAR(reflect::cast<double>(features.at("a").value()), 0.2, 1e-7);
    }
}

TEST_F(IOTest, roundtrip_read_write)
{
    auto features = read("test_data/simple_test.grr");

    EXPECT_EQ(features.size(), 4);
    EXPECT_NEAR(reflect::cast<double>(features.at("d").value().get("value")), 0.5, 1e-7);
    EXPECT_NEAR(reflect::cast<double>(features.at("c").value().get("value")), 0.3, 1e-7);
    EXPECT_NEAR(reflect::cast<double>(features.at("b").value().get("value")), 0.1, 1e-7);
    EXPECT_NEAR(reflect::cast<double>(features.at("a").value().get("value")), 0.2, 1e-7);

    auto y = details::feature_tree_to_yaml(features.at("d"));
    test_basic_tree(y, "add", "MyDouble");
}
