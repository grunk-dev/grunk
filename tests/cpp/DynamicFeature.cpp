#include <gtest/gtest.h>

#include <grunk/grunk.hpp>

using namespace grunk;

namespace {

struct MyDouble {
    double val {0.75};
};

// a class with features and non-features as members
struct MyStruct {

    MyStruct(double v)
     : val{v} 
    {}

    double times(double factor) const {
        return val*factor;
    }

    double timesc(double factor) {
        return val*factor;
    }

    double val;
};

} //namespace

class DynamicFeatureTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        reflect::register_type<double>("double")
        .add_constructor<double>();

        reflect::register_type<MyDouble>("MyDouble")
        .add_constructor<>()
        .add_data_member(&MyDouble::val, "val");;

        reflect::register_type<MyStruct>("MyStruct")
        .add_constructor<double>()
        .add_data_member(&MyStruct::val, "val")
        .add_member_function(&MyStruct::times, "times")
        .add_member_function(&MyStruct::timesc, "timesc");
    } 

    static void TearDownTestCase() {
        reflect::get_type_registry().clear();
        reflect::get_function_registry().clear();
    } 
};

TEST_F(DynamicFeatureTest, ctor)
{
    Feature<double> x("x", 0.25);
    auto y = grunk::action("y", "MyStruct", x).output();

    EXPECT_FALSE(y.is_valid());
    EXPECT_EQ(reflect::cast<MyStruct>(y.value()).val, 0.25);
    EXPECT_TRUE(y.is_valid());

    x.access_value() = 0.75;

    EXPECT_FALSE(y.is_valid());
    EXPECT_EQ(reflect::cast<MyStruct>(y.value()).val, 0.75);
    EXPECT_TRUE(y.is_valid());
}

TEST_F(DynamicFeatureTest, get)
{
    Feature x("x", "MyStruct", 0.5);

    Feature v = grunk::action("v", "MyStruct::val", x).output();
    EXPECT_EQ(reflect::cast<double>(v.value()), 0.5);

     x.access_value().set("val", 0.3);
     EXPECT_FALSE(v.is_valid());
     EXPECT_EQ(reflect::cast<double>(v.value()), 0.3);
}

TEST_F(DynamicFeatureTest, invoke)
{
    Feature x("x", "MyStruct", 0.5);
    Feature factor("factor", "double", 3.);

    Feature v = grunk::action("v", "MyStruct::times", x, factor).output();
    
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(reflect::cast<double>(v.value()), 1.5, 1e-12);
    EXPECT_TRUE(v.is_valid());

    x.access_value().set("val", 0.3);
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(reflect::cast<double>(v.value()), 0.9, 1e-12);
    EXPECT_TRUE(v.is_valid());

    factor.access_value() = 2.;
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(reflect::cast<double>(v.value()), 0.6, 1e-12);
    EXPECT_TRUE(v.is_valid());
}

TEST_F(DynamicFeatureTest, invoke_nonConstMemberFun)
{
    Feature x("x", "MyStruct", 0.5);
    Feature factor("factor", "double", 3.);
    EXPECT_THROW(grunk::action("", "MyStruct::timesc", x, factor), reflect::Unresolvable);
}
