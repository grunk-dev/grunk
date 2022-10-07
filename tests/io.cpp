#include <gtest/gtest.h>

#include <grunk/grunk.h>
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
        .AddConstructor<int>();

        register_function(
            [](double const& l, double const& r){ return l+r;}, 
            "plus"
        );
    } 

};

TEST_F(IOTest, no_serialize_method)
{
    auto x = Feature("x", "NonSerializable", 42);
    EXPECT_THROW(x.param().node_pointer()->serialize(), std::invalid_argument);
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
    auto x = Reflect::DynamicObject(1.23);
    auto sx = parametric::serialize(x);
    auto y = YAML::Load(sx);
#ifdef YAML_TAG_WORKAROUND
    EXPECT_EQ(y.size(), 2);
    EXPECT_EQ(y["tag"].as<std::string>(), "double");
    EXPECT_NEAR(y["value"].as<double>(), 1.23, 1e-6);
#else 
    EXPECT_EQ(y.Tag(), "double");
    EXPECT_NEAR(y.as<double>(), 1.23, 1e-6);
#endif

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
#ifdef YAML_TAG_WORKAROUND
    EXPECT_EQ(yc.size(), 2);
    EXPECT_EQ(yc["tag"].as<std::string>(), "add");

    //two arrays, one for outputs one for inputs
    EXPECT_EQ(yc["value"].size(), 2);

    //outputs
    EXPECT_EQ(yc["value"][0].size(), 1);
    EXPECT_EQ(yc["value"][0][0].as<std::string>(), "z");

    //inputs
    EXPECT_EQ(yc["value"][1].size(), 2);
    EXPECT_EQ(yc["value"][1][0].as<std::string>(), "x");
    EXPECT_EQ(yc["value"][1][1].as<std::string>(), "y");
#else 
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
#endif

    // Can't serialize algorithm with non registered function
    EXPECT_THROW(w->serialize(), std::logic_error);

    // dependent parameter
    auto zn = z->get().param().node_pointer()->serialize();
    EXPECT_EQ(zn, "");
}

TEST_F(IOTest, feature_tree_to_yaml_empty)
{
    auto x = details::feature_tree_to_yaml();

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
        write("test.gk", d);
    }

    auto y = YAML::LoadFile("test.gk");
    test_basic_tree(y, "plus", "double");
}

TEST_F(IOTest, write_const_iterable_container)
{
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("b", "double", 0.1);
        auto c = eval("c", "plus", a, b)->get();
        auto d = eval("d", "plus", c, a)->get();

        std::vector<RuntimeFeature> v{a,b,c,d};
        write("test_vector.gk", v);

        FeatureContainer m;
        m.insert({"a", a});
        m.insert({"b", b});
        m.insert({"c", c});
        m.insert({"d", d});
        write("test_unordered_map.gk", m);
    }

    auto yv = YAML::LoadFile("test_vector.gk");
    test_basic_tree(yv, "plus", "double");

    auto ym = YAML::LoadFile("test_unordered_map.gk");
    test_basic_tree(ym, "plus", "double");
}

TEST_F(IOTest, deserialize_double)
{
    YAML::Node y;
    y["double"] = 0.9876;

    YAML::const_iterator it=y.begin();
    auto d = details::deserialize(
        it->first.as<std::string>(),
        it->second
    );
    EXPECT_NEAR(Reflect::cast<double>(d), 0.9876, 1e-7);
}

TEST_F(IOTest, deserialize_simple_plugin_MyDouble)
{
    YAML::Node y;
    y["MyDouble"] = 0.55557;

    YAML::const_iterator it=y.begin();
    auto md = details::deserialize(
        it->first.as<std::string>(),
        it->second
    );
    auto d = md.get("value");
    EXPECT_NEAR(Reflect::cast<double>(d), 0.55557, 1e-7);
}

TEST_F(IOTest, no_deserialize_method)
{
    YAML::Node y;
    y["NonSerializable"] = "doesnt matter what I write here";

    YAML::const_iterator it=y.begin();
    EXPECT_THROW(
        details::deserialize(
            it->first.as<std::string>(),
            it->second
        ),
        grunk::io_error
    );
}

TEST_F(IOTest, deserialize_non_existing_type)
{
    YAML::Node y;
    y["NonExistentType"] = 0.33;

    YAML::const_iterator it=y.begin();
    EXPECT_THROW(
        details::deserialize(
            it->first.as<std::string>(),
            it->second
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
#ifdef YAML_TAG_WORKAROUND
    auto as = YAML::Load(parametric::serialize(Reflect::DynamicObject(13.2)));
    parameters["a"] = as["value"];
    parameters["a"].SetTag(as["tag"].as<std::string>());
    auto bs = YAML::Load(parametric::serialize(Reflect::DynamicObject(11.8)));
    parameters["b"] = bs["value"];
    parameters["b"].SetTag(bs["tag"].as<std::string>());
#else 
    parameters["a"] = YAML::Load(parametric::serialize(Reflect::DynamicObject(13.2)));
    parameters["b"] = YAML::Load(parametric::serialize(Reflect::DynamicObject(11.8)));
#endif
    root["parameters"] = parameters;

    YAML::Node steps;

    YAML::Node step;
    step["spunck"] = YAML::Node();
    step["spunck"].push_back(std::vector<std::string>{"c"});
    step["spunck"].push_back(std::vector<std::string>{"a", "b"});
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
    parameters["a"] = YAML::Load(parametric::serialize(Reflect::DynamicObject(13.2)));
    parameters["b"] = YAML::Load(parametric::serialize(Reflect::DynamicObject(11.8)));
    root["parameters"] = parameters;

    YAML::Node steps;

    YAML::Node step2;
    step2["function"] = "add";
    step2["inputs"] = std::vector<std::string>{"a", "c"};
    step2["outputs"] = std::vector<std::string>{"d"};
    steps.push_back(step2);

    YAML::Node step1;
    step1["function"] = "add";
    step1["inputs"] = std::vector<std::string>{"a", "b"};
    step1["outputs"] = std::vector<std::string>{"c"};
    steps.push_back(step1);

    root["steps"] = steps;

    EXPECT_THROW(details::yaml_to_feature_tree(root), grunk::io_error);
}

TEST_F(IOTest, roundtrip_write_read)
{
    {
        auto a = Feature("a", "double", 0.2);
        auto b = Feature("b", "double", 0.1);
        auto c = eval("c", "plus", a, b)->get();
        auto d = eval("d", "plus", c, a)->get();
        write("test.gk", d);
    }

    {
        auto features = read("test.gk");
        EXPECT_EQ(features.size(), 4);
        EXPECT_NEAR(Reflect::cast<double>(features.at("d").value()), 0.5, 1e-7);
        EXPECT_NEAR(Reflect::cast<double>(features.at("c").value()), 0.3, 1e-7);
        EXPECT_NEAR(Reflect::cast<double>(features.at("b").value()), 0.1, 1e-7);
        EXPECT_NEAR(Reflect::cast<double>(features.at("a").value()), 0.2, 1e-7);
    }
}

TEST_F(IOTest, roundtrip_read_write)
{
    auto features = read("test_data/simple_test.gk");

    EXPECT_EQ(features.size(), 4);
    EXPECT_NEAR(Reflect::cast<double>(features.at("d").value().get("value")), 0.5, 1e-7);
    EXPECT_NEAR(Reflect::cast<double>(features.at("c").value().get("value")), 0.3, 1e-7);
    EXPECT_NEAR(Reflect::cast<double>(features.at("b").value().get("value")), 0.1, 1e-7);
    EXPECT_NEAR(Reflect::cast<double>(features.at("a").value().get("value")), 0.2, 1e-7);

    auto y = details::feature_tree_to_yaml(features.at("d"));
    test_basic_tree(y, "add", "MyDouble");
}