#include <gtest/gtest.h>

#include <grunk/runtime/RuntimeFeature.h>

using namespace grunk;

namespace runtime_feature_test {

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

} //namespace runtime_feature_test

using namespace runtime_feature_test;

class RuntimeFeatureTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        Reflect::Reflect<double>("double")
        .AddConstructor<double>();

        Reflect::Reflect<MyDouble>("MyDouble")
        .AddConstructor<>()
        .AddDataMember(&MyDouble::val, "val");;

        Reflect::Reflect<MyStruct>("MyStruct")
        .AddConstructor<double>()
        .AddDataMember(&MyStruct::val, "val")
        .AddMemberFunction(&MyStruct::times, "times")
        .AddMemberFunction(&MyStruct::timesc, "timesc");
    } 

    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};

TEST_F(RuntimeFeatureTest, Conversions)
{
    RuntimeFeature x("MyStruct", 0.33);
    EXPECT_NEAR(x.value().get("val").cast<double>(), 0.33, 1e-12);

    // Feature<RuntimeObject> -> Feature<T>
    Feature<MyStruct> y(x);
    EXPECT_NEAR(y.value().val, 0.33, 1e-12);

    // Feature<T> -> Feature<RuntimeObject> 
    RuntimeFeature z(y);
    EXPECT_NEAR(z.value().get("val").cast<double>(), 0.33, 1e-12);
}

TEST_F(RuntimeFeatureTest, get)
{
    Feature x("MyStruct", 0.5);

    Feature v = x.get("val")->get();
    EXPECT_EQ(v.value().cast<double>(), 0.5);

     x.access_value().set("val", 0.3);
     EXPECT_FALSE(v.is_valid());
     EXPECT_EQ(v.value().cast<double>(), 0.3);

}

TEST_F(RuntimeFeatureTest, invoke)
{
    Feature x("MyStruct", 0.5);
    Feature factor("double", 3.);

    Feature v = x.invoke("times", factor)->get();
    
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(v.value().cast<double>(), 1.5, 1e-12);
    EXPECT_TRUE(v.is_valid());

    x.access_value().set("val", 0.3);
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(v.value().cast<double>(), 0.9, 1e-12);
    EXPECT_TRUE(v.is_valid());

    factor.access_value() = (RuntimeObject)2.;
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(v.value().cast<double>(), 0.6, 1e-12);
    EXPECT_TRUE(v.is_valid());
}

TEST_F(RuntimeFeatureTest, invoke_nonConstMemberFun)
{
    Feature x("MyStruct", 0.5);
    Feature factor("double", 3.);
    Feature v = x.invoke("timesc", factor)->get();
    EXPECT_THROW(v.value(), Reflect::disregards_qualifier);
}
