#include <gtest/gtest.h>

#include <parametric/core.hpp>
#include <grunk/Feature.h>
#include <grunk/Algorithm.h>

using namespace grunk;

namespace feature_test {

struct MyDouble {
    double val {0.75};
};

// a class with features and non-features as members
struct MyStruct {

    MyStruct(double v)
     : feature(RuntimeFeature("MyDouble"))
     , val{v} 
    {}

    double times(double factor) const {
        return val*factor;
    }

    RuntimeFeature feature;

    double val;
};

} //namespace feature_test

using namespace feature_test;

class FeatureTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        Reflect::Reflect<RuntimeFeature>("Feature");

        Reflect::Reflect<double>("double");

        Reflect::Reflect<MyDouble>("MyDouble")
        .AddConstructor<>()
        .AddDataMember(&MyDouble::val, "val");;

        Reflect::Reflect<MyStruct>("MyStruct")
        .AddConstructor<double>()
        .AddDataMember(&MyStruct::val, "val")
        .AddDataMember(&MyStruct::feature, "feature")
        .AddMemberFunction(&MyStruct::times, "times");
    } 

    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};

TEST_F(FeatureTest, Compiletime_invoke_data_member)
{
    Feature x(MyStruct(0.5));

    Feature v = x.invoke([](MyStruct const& m){ return m.val; })->get();
    EXPECT_EQ(v.value(), 0.5);

    x.access_value().val = 0.3;
    EXPECT_FALSE(v.is_valid());
    EXPECT_EQ(v.value(), 0.3);

}

TEST_F(FeatureTest, Compiletime_invoke_member_function)
{
    Feature x(MyStruct(0.5));
    Feature factor(3.);

    Feature v = x.invoke([](MyStruct const& m, double factor){ return m.times(factor); }, factor)->get();
    
    EXPECT_NEAR(v.value(), 1.5, 1e-12);
    EXPECT_TRUE(v.is_valid());

    x.access_value().val = 0.3;
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(v.value(), 0.9, 1e-12);
    EXPECT_TRUE(v.is_valid());

    factor.access_value() = 2.;
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(v.value(), 0.6, 1e-12);
    EXPECT_TRUE(v.is_valid());
}

// TEST_F(FeatureTest, GetValueAsValue)
// {
//     Feature x("MyStruct", 0.5);
//     double val_as_value = x.GetAs<double>("val");

//     EXPECT_EQ(val_as_value, 0.5);
//     EXPECT_TRUE(x.is_valid());

//     x.AccessValue().Set("val", 0.25);
//     EXPECT_EQ(x.GetAs<double>("val"), 0.25);

//     EXPECT_TRUE(x.is_valid());
// }

// TEST_F(FeatureTest, GetValueAsFeature)
// {
//     Feature x("MyStruct", 0.5);

//     Feature val_as_feature = x.Get("val");

//     EXPECT_FALSE(val_as_feature.is_valid());

//     EXPECT_EQ(val_as_feature.cast<double>(), 0.5);
//     EXPECT_TRUE(val_as_feature.is_valid());

//     x.AccessValue().Set("val", 0.25);
//     EXPECT_EQ(x.GetAs<double>("val"), 0.25);

//     EXPECT_TRUE(x.is_valid());
//     EXPECT_FALSE(val_as_feature.is_valid());

//     EXPECT_EQ(val_as_feature.cast<double>(), 0.25);
//     EXPECT_TRUE(val_as_feature.is_valid());
// }

//TODO the following doesn't work yet

// TEST_F(FeatureTest, GetFeatureAsFeature)
// {
//     // make sure we .Get doesn't return a Feature of a feature,
//     // but does a proper monadic join
//     auto x = Feature("MyStruct", 0.5);
//     Feature f = x.Get("feature");

//     ASSERT_FALSE(f.is_valid());

//     ASSERT_EQ(f.Value().GetTypeInfo()->GetName(), "MyDouble");
//     EXPECT_EQ(f.GetAs<double>("val"), 0.75);
// }