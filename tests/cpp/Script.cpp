#include <gtest/gtest.h>

#include <grunk/grunk.hpp>

namespace {

    struct Pnt
    {
        Pnt() = default;

        inline void set_x(double x_) { x = x_; }
        inline void set_y(double y_) { y = y_; }
        inline void set_z(double z_) { z = z_; }

        double x{0.};
        double y{0.};
        double z{0.};
    };

}

class ScriptTest : public ::testing::Test
{
public:
    static void SetUpTestCase() {

        grunk::init();

        reflect::register_type<Pnt>("Pnt")
        .add_constructor<>()
        .add_member_function(&Pnt::set_x, "set_x")
        .add_member_function(&Pnt::set_y, "set_y")
        .add_member_function(&Pnt::set_z, "set_z")
        .add_data_member(&Pnt::x, "x")
        .add_data_member(&Pnt::y, "y")
        .add_data_member(&Pnt::z, "z");
    }

    static void TearDownTestCase() {
        reflect::get_type_registry().clear();
        reflect::get_function_registry().clear();
    } 
};

TEST_F(ScriptTest, basic_usage)
{
    grunk::Feature u("u", "double", 0.1);
    grunk::Feature v("v", "double", 0.2);

    // calling a non-const member function as an action should fail: 
    // we are not allowed to modify input arguments
    grunk::DynamicFeature p("p", "Pnt");
    EXPECT_THROW(
        grunk::action("should_fail", "Pnt::set_x", p, u),
        reflect::Unresolvable
    );

    // We can isolate the non-const getters in a grunk::script
    auto s = grunk::script(
        {
            {"Pnt", {"p"}, {}},                 // create a new point p
            {"Pnt::set_x", {}, {"p", u}},       // invoke non-const setter 
            {"Pnt::set_y", {}, {"p", v}},       // invoke non-const setter
        },
        {"p"}                                   // return new point p
    );

    EXPECT_EQ(s.size(), 1);
    auto pnt = s.output().value().as<Pnt>();
    EXPECT_EQ(pnt.x, 0.1);
    EXPECT_EQ(pnt.y, 0.2);
    EXPECT_EQ(pnt.z, 0.0);

    EXPECT_TRUE(s.output().get_type_descriptor());
}

TEST_F(ScriptTest, no_inputs)
{
    // issue #88 on gitlab
    auto s = grunk::script(
        {
            {"Pnt", {"p"}, {}},
        },
        {"p"}
    );
    auto yml = grunk::serialize(s.output());
    EXPECT_EQ(yml["parameters"].size(), 0);
    EXPECT_EQ(yml["steps"].size(), 1);
}

TEST_F(ScriptTest, serialize)
{
    grunk::Feature u("u", "double", 0.1);
    grunk::Feature v("v", "double", 0.2);
    auto s = grunk::script(
        {
            {"Pnt", {"p"}, {}},                 // create a new point p
            {"Pnt::set_x", {}, {"p", u}},       // invoke non-const setter 
            {"Pnt::set_y", {}, {"p", v}},       // invoke non-const setter
        },
        {"p"}                                   // return new point p
    );
    
    YAML::Node node = serialize(u, v, s.output());

     EXPECT_EQ(node["steps"].size(), 1);
     auto script = node["steps"][0];
     EXPECT_EQ(script.Tag(), "script");
     EXPECT_EQ(script.size(), 2);
     EXPECT_TRUE(script["steps"]);
     EXPECT_EQ(script["steps"].size(), 3);

     // - !<Pnt> [[p], ~]
     EXPECT_EQ(script["steps"][0].size(), 2);
     EXPECT_EQ(script["steps"][0].Tag(), "Pnt");
     EXPECT_EQ(script["steps"][0][0].size(), 1);
     EXPECT_EQ(script["steps"][0][0][0].as<std::string>(), "p");
     EXPECT_EQ(script["steps"][0][1].Type(), YAML::NodeType::Null);

     // - !<Pnt::set_x> [~, [p, u]]
     EXPECT_EQ(script["steps"][1].size(), 2);
     EXPECT_EQ(script["steps"][1].Tag(), "Pnt::set_x");
     EXPECT_EQ(script["steps"][1][0].Type(), YAML::NodeType::Null);
     EXPECT_EQ(script["steps"][1][1].size(), 2);
     EXPECT_EQ(script["steps"][1][1][0].as<std::string>(), "p");
     EXPECT_EQ(script["steps"][1][1][1].as<std::string>(), "u");

     // - !<Pnt::set_y> [~, [p, v]]
     EXPECT_EQ(script["steps"][2].size(), 2);
     EXPECT_EQ(script["steps"][2].Tag(), "Pnt::set_y");
     EXPECT_EQ(script["steps"][2][0].Type(), YAML::NodeType::Null);
     EXPECT_EQ(script["steps"][2][1].size(), 2);
     EXPECT_EQ(script["steps"][2][1][0].as<std::string>(), "p");
     EXPECT_EQ(script["steps"][2][1][1].as<std::string>(), "v");

     EXPECT_TRUE(script["returns"]);
     EXPECT_EQ(script["returns"].size(), 1);
     EXPECT_EQ(script["returns"][0].as<std::string>(), "p");
}

TEST_F(ScriptTest, deserialize)
{
    YAML::Node node;

    {
        grunk::Feature u("u", "double", 0.1);
        grunk::Feature v("v", "double", 0.2);
        auto s = grunk::script(
            {
                {"Pnt", {"p"}, {}},                 // create a new point p
                {"Pnt::set_x", {}, {"p", u}},       // invoke non-const setter
                {"Pnt::set_y", {}, {"p", v}},       // invoke non-const setter
            },
            {"p"}                                   // return new point p
        );

        node = serialize(u, v, s.output());
    }

    auto d = grunk::Recipe::deserialize(node);

    EXPECT_EQ(d.num_features(), 3);
    EXPECT_NEAR(d["u"].value().as<double>(), 0.1, 1e-10);
    EXPECT_NEAR(d["v"].value().as<double>(), 0.2, 1e-10);
    auto pnt = d["p"].value().as<Pnt>();
    EXPECT_NEAR(pnt.x, 0.1, 1e-10);
    EXPECT_NEAR(pnt.y, 0.2, 1e-10);
    EXPECT_NEAR(pnt.z, 0.0, 1e-10);
}

TEST_F(ScriptTest, constants)
{
    {
        grunk::Feature u("", "double", 0.1);
        grunk::Feature v("", "double", 0.2);
        auto s = grunk::script(
            {
                {"Pnt", {"p"}, {}},                 // create a new point p
                {"Pnt::set_x", {}, {"p", u}},       // invoke non-const setter
                {"Pnt::set_y", {}, {"p", v}},       // invoke non-const setter
            },
            {"p"}                                   // return new point p
        );

        YAML::Node node = serialize(s.output());
        EXPECT_FALSE(node["parameters"]);
        EXPECT_EQ(node["steps"][0]["steps"][1][1][1].Tag(), "double");
        EXPECT_EQ(node["steps"][0]["steps"][2][1][1].Tag(), "double");

        grunk::write("tmp.grr.yml", s.output());
    }

    auto r = grunk::read("tmp.grr.yml");
    EXPECT_EQ(r.num_features(), 1);
    EXPECT_EQ(r["p"].value().as<Pnt>().x, 0.1);
    EXPECT_EQ(r["p"].value().as<Pnt>().y, 0.2);

}