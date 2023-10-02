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

        grunk::StdPlugin().init();

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

}