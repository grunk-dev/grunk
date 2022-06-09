#include <gtest/gtest.h>

#include <parametric/core.hpp>
#include <grunk/Feature.h>

using namespace grunk;

namespace feature_test {

struct MyDouble {
    double val {0.75};
};

// a class with one const and one non-const member function
struct MyStruct {

    MyStruct(double v)
     : feature(Feature("MyDouble"))
     , val{v} 
    {}

    Feature feature;

    double val;
};

} //namespace feature_test

using namespace feature_test;

class FeatureTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        Reflect::Reflect<Feature>("Feature");

        Reflect::Reflect<double>("double");

        Reflect::Reflect<MyDouble>("MyDouble")
        .AddConstructor<>()
        .AddDataMember(&MyStruct::val, "val");;

        Reflect::Reflect<MyStruct>("MyStruct")
        .AddConstructor<double>()
        .AddDataMember(&MyStruct::val, "val")
        .AddDataMember(&MyStruct::feature, "feature");;
    } 

    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};

TEST_F(FeatureTest, MemberGetter)
{
    Feature x("MyStruct", 0.5);

    Feature val_as_feature = x.Get("val");

    double val_as_value = x.GetAs<double>("val");
    EXPECT_EQ(val_as_value, 0.5);

    EXPECT_TRUE(x.is_valid());
    EXPECT_FALSE(val_as_feature.is_valid());

    EXPECT_EQ(val_as_feature.cast<double>(), val_as_value);
    EXPECT_TRUE(val_as_feature.is_valid());

    x.value().Set("val", 0.25);
    EXPECT_EQ(x.GetAs<double>("val"), 0.25);

    EXPECT_TRUE(x.is_valid());
    EXPECT_FALSE(val_as_feature.is_valid());

    EXPECT_EQ(val_as_feature.cast<double>(), 0.25);
    EXPECT_TRUE(val_as_feature.is_valid());
}

TEST_F(FeatureTest, FeatureMember)
{
    // make sure we .Get doesn't return a Feature of a feature,
    // but does a proper monadic join
    auto x = Feature("MyStruct", 0.5);
    Feature f = x.Get("feature");

    // auto shit = f.value();//.GetTypeInfo();//->GetName();
    ASSERT_EQ(f.value().GetTypeInfo()->GetName(), "MyDouble");
    EXPECT_EQ(f.GetAs<double>("val"), 0.75);
}