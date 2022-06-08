#include <gtest/gtest.h>

#include <parametric/core.hpp>
#include <grunk/Algorithm.h>

using namespace grunk;

// a class with one const and one non-const member function
struct MyDouble {

    MyDouble(double v) : val{v} {}

    double val;
};


// normal function
MyDouble add(MyDouble const& l, MyDouble const& r) {
    return MyDouble(l.val + r.val);
}


class AlgorithmTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {
        Reflect::Reflect<double>("double");

        Reflect::Reflect<MyDouble>("MyDouble")
        .AddConstructor<double>()
        .AddDataMember(&MyDouble::val, "val");
    } 

    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};

TEST_F(AlgorithmTest, BasicUsage)
{
    auto f = RuntimeFunction(&add);

    // l and r are the root input nodes
    auto l = parametric::new_param(make_rto("MyDouble", 0.2));
    auto r = parametric::new_param(make_rto("MyDouble", 0.1));

    // a depends on l and r, b depends on a and r
    //    
    //   l      r
    //    \   / |
    //      a   |
    //       \  |
    //         b
    //
    auto a = new_algorithm(f, {l, r})->get();
    auto b = new_algorithm(f, {a, r})->get();

    // nothing has been computed yet, we just registered the feature tree
    EXPECT_FALSE(a.is_valid());
    EXPECT_FALSE(b.is_valid());

    // evaluating b should trigger evaluation of the entire tree
    EXPECT_NEAR(b.value().Get("val").cast<double>(), 0.4, 1e-12);

    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(b.is_valid());

    EXPECT_NEAR(a.value().Get("val").cast<double>(), 0.3, 1e-12);

    // reseting a root node should invalidate the entire tree
    l.change_value().Set("val", 0.5); 

    // TODO: using set_value() instead of change_value() does not seem to work

    EXPECT_FALSE(a.is_valid());
    EXPECT_FALSE(b.is_valid());

    // evaluating b should trigger evaluation of the entire tree
    EXPECT_NEAR(b.value().Get("val").cast<double>(), 0.7, 1e-12);

    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(b.is_valid());

    EXPECT_NEAR(a.value().Get("val").cast<double>(), 0.6, 1e-12);
}

// TODO: test Algorithm for multi-output runtime function