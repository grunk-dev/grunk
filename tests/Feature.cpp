#include <gtest/gtest.h>

#include <grunk/core/Feature.h>

using namespace grunk;

namespace feature_test {

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

} //namespace feature_test

using namespace feature_test;

TEST(FeatureTest, get)
{
    Feature x(MyStruct(0.5));

    Feature v = x.get(&MyStruct::val)->get();
    EXPECT_EQ(v.value(), 0.5);

    x.access_value().val = 0.3;
    EXPECT_FALSE(v.is_valid());
    EXPECT_EQ(v.value(), 0.3);

}

TEST(FeatureTest, invoke)
{
    Feature x(MyStruct(0.5));
    Feature factor(3.);

    Feature v = x.invoke(&MyStruct::times, factor)->get();
    
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(v.value(), 1.5, 1e-12);
    EXPECT_TRUE(v.is_valid());

    x.access_value().val = 0.3;
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(v.value(), 0.9, 1e-12);
    EXPECT_TRUE(v.is_valid());

    // a dependent node can be manually overwritten,
    // but a change further down the tree will get precedence.
    v.access_value() = 0.12345;
    EXPECT_TRUE(v.is_valid());
    EXPECT_EQ(v.value(), 0.12345);

    factor.access_value() = 2.;
    EXPECT_FALSE(v.is_valid());
    EXPECT_NEAR(v.value(), 0.6, 1e-12);
    EXPECT_TRUE(v.is_valid());
}