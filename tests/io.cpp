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

TEST_F(IOTest, parse_feature_tree_empty)
{
    auto x = details::parse_feature_tree();

    // there should be just one node called "uses"
    EXPECT_EQ(x.size(), 1);
    EXPECT_EQ(x.begin()->first.as<std::string>(), "uses");
    auto uses = x["uses"];
    
    // in "uses", there should be just one node called "grunk"
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
    auto a = Feature("a", 0.2);
    auto b = Feature("b", 0.1);
    auto c = eval("c", "plus", a, b)->get();
    auto d = eval("d", "plus", c, a)->get();

    std::cout << to_string(d) << std::endl;
}

TEST_F(IOTest, simple_plugin)
{
    auto a = Feature("a", "MyDouble", 0.2);
    auto b = Feature("b", "MyDouble", 0.1);
    auto c = eval("c", "add", a, b)->get();
    auto d = eval("d", "add", c, a)->get();

    std::cout << to_string(d) << std::endl;
}

// To Do:
//  - segfault in plugin registry dtor
//  - test serialize overloads and specializations of basic types, algorithms
//  - actually test something in basic and simple_plugin
//  - test error message if overload or specialization is missing.
//  - test writing to file
//  - test reading from file 
//  - test deserializing a type
//  - test deserialize error on nonexistent function
//  - test deserialize error on nonexistent type
//  - test roundtrip starting from tree
//  - test roundtrip starting from file