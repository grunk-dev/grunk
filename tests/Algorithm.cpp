#include <gtest/gtest.h>

#include <parametric/core.hpp>
#include <grunk/Algorithm.h>

using namespace grunk;

namespace Algorithm_test {

// a class with one const and one non-const member function
struct MyDouble {

    MyDouble(double v) : val{v} {}

    double& value_ref() {
        return val;
    }

    double val;
};


// normal function
MyDouble add(MyDouble const& l, MyDouble const& r) {
    return MyDouble(l.val + r.val);
}

struct Point {
    Point(double xin, double yin) : x(xin), y(yin) {}
    double x;
    double y;
};

// simple multi-output function
std::tuple<double, double> get_components(Point const& p){
    return std::make_tuple(p.x, p.y);
}

} //namespace Algorithm_test

using namespace Algorithm_test;


class AlgorithmTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {
        Reflect::Reflect<double>("double");

        Reflect::Reflect<MyDouble>("MyDouble")
        .AddConstructor<double>()
        .AddDataMember(&MyDouble::val, "val");

        Reflect::Reflect<Point>("Point")
        .AddConstructor<double, double>()
        .AddDataMember(&Point::x, "x")
        .AddDataMember(&Point::y, "y");
    } 

    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};

TEST_F(AlgorithmTest, RuntimeBasic)
{
    auto f = RuntimeFunction(&add);

    // l and r are the root input nodes
    auto l = Feature("MyDouble", 0.2);
    auto r = Feature("MyDouble", 0.1);

    // a depends on l and r, b depends on a and r
    //    
    //   l      r
    //    \   / |
    //      a   |
    //       \  |
    //         b
    //
    auto a = eval(f, l, r)->get();
    auto b = eval(f, a, r)->get();

    // nothing has been computed yet, we just registered the feature tree
    EXPECT_FALSE(a.is_valid());
    EXPECT_FALSE(b.is_valid());

    // evaluating b should trigger evaluation of the entire tree
    EXPECT_NEAR(b.value().Get("val").cast<double>(), 0.4, 1e-12);

    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(b.is_valid());

    EXPECT_NEAR(a.value().Get("val").cast<double>(), 0.3, 1e-12);

    // reseting a root node should invalidate the entire tree
    l.access_value().Set("val", 0.5);

    // TODO: using set_value() instead of change_value() does not seem to work

    EXPECT_FALSE(a.is_valid());
    EXPECT_FALSE(b.is_valid());

    // evaluating b should trigger evaluation of the entire tree
    EXPECT_NEAR(b.value().Get("val").cast<double>(), 0.7, 1e-12);

    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(b.is_valid());

    EXPECT_NEAR(a.value().Get("val").cast<double>(), 0.6, 1e-12);
}

TEST_F(AlgorithmTest, CompiletimeBasic)
{
    // l and r are the root input nodes
    auto l = Feature(MyDouble(0.2));
    auto r = Feature(MyDouble(0.1));

    // a depends on l and r, b depends on a and r
    //    
    //   l      r
    //    \   / |
    //      a   |
    //       \  |
    //         b
    //
    auto a = eval(&add, l, r)->get();
    auto b = eval(&add, a, r)->get();

    // nothing has been computed yet, we just registered the feature tree
    EXPECT_FALSE(a.is_valid());
    EXPECT_FALSE(b.is_valid());

    // evaluating b should trigger evaluation of the entire tree
    EXPECT_NEAR(b.value().val, 0.4, 1e-12);

    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(b.is_valid());

    EXPECT_NEAR(a.value().val, 0.3, 1e-12);

    // reseting a root node should invalidate the entire tree
    l.access_value().val = 0.5; 

    // TODO: using set_value() instead of change_value() does not seem to work

    EXPECT_FALSE(a.is_valid());
    EXPECT_FALSE(b.is_valid());

    // evaluating b should trigger evaluation of the entire tree
    EXPECT_NEAR(b.value().val, 0.7, 1e-12);

    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(b.is_valid());

    EXPECT_NEAR(a.value().val, 0.6, 1e-12);
}

TEST_F(AlgorithmTest, PassNonRumtimeFeatureToRuntimeAlgorithm)
{
    auto f = RuntimeFunction(&add);

    auto lr = Feature("MyDouble", 0.2);
    auto lc = Feature(MyDouble(0.2));
    auto rc = Feature(MyDouble(0.1));

    // (RuntimeFeature, Feature<T>) -> RuntimeAlgorithm
    auto ret1 = eval(f, lr, rc)->get();
    EXPECT_NEAR(ret1.value().Get("val").cast<double>(), 0.3, 1e-12);

    // (Feature<T>, Feature<T>) -> RuntimeAlgorithm
    auto ret2 = eval(f, lc, rc)->get();
    EXPECT_NEAR(ret2.value().Get("val").cast<double>(), 0.3, 1e-12);
}

TEST_F(AlgorithmTest, CompiletimeMultiOutput)
{
    auto f = &get_components;

    auto i = Feature(Point(0.2, 0.6));
    auto x = eval(f, i)->get<0>();
    auto y = eval(f, i)->get<1>();

    EXPECT_EQ(x.value(), 0.2);
    EXPECT_EQ(y.value(), 0.6);

    i.access_value().y = 0.7;
    
    EXPECT_FALSE(x.is_valid());
    EXPECT_FALSE(y.is_valid());

    EXPECT_EQ(y.value(), 0.7);
}

TEST_F(AlgorithmTest, RuntimeMultiOutput)
{
    auto f = grunk::RuntimeFunction(&get_components);

    auto i = Feature("Point", 0.2, 0.6);
    auto x = eval(f, i)->get<0>();
    auto y = eval(f, i)->get<1>();

    EXPECT_EQ(x.value().cast<double>(), 0.2);
    EXPECT_EQ(y.value().cast<double>(), 0.6);

    i.access_value().Set("y", 0.7);
    
    EXPECT_FALSE(x.is_valid());
    EXPECT_FALSE(y.is_valid());

    EXPECT_EQ(y.value().cast<double>(), 0.7);
}

// TEST_F(AlgorithmTest, CompiletimeReturnReference)
// {
//     Feature x(MyDouble{0.125});
//     auto v = eval(&MyDouble::value_ref, x)->get();
// }
