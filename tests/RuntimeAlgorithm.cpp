#include <gtest/gtest.h>

#include <grunk/dynamic/RuntimeAlgorithm.h>

using namespace grunk;

namespace RuntimeAlgorithm_test {

// a class with one const and one non-const member function
struct MyDouble {

    MyDouble(double v) : val{v} {}

    double const& value_ref() const {
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

using namespace RuntimeAlgorithm_test;

class RuntimeAlgorithmTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {
        Reflect::Reflect<double>("double")
        .AddConstructor<double>();

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

TEST_F(RuntimeAlgorithmTest, Basic)
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
    EXPECT_NEAR(b.value().get_as<double>("val"), 0.4, 1e-12);

    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(b.is_valid());

    EXPECT_NEAR(a.value().get_as<double>("val"), 0.3, 1e-12);

    // reseting a root node should invalidate the entire tree
    l.access_value().set("val", 0.5);

    // TODO: using set_value() instead of change_value() does not seem to work

    EXPECT_FALSE(a.is_valid());
    EXPECT_FALSE(b.is_valid());

    // evaluating b should trigger evaluation of the entire tree
    EXPECT_NEAR(b.value().get_as<double>("val"), 0.7, 1e-12);

    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(b.is_valid());

    EXPECT_NEAR(a.value().get_as<double>("val"), 0.6, 1e-12);
}

TEST_F(RuntimeAlgorithmTest, PassNonRumtimeFeatureToRuntimeAlgorithm)
{
    auto f = RuntimeFunction(&add);

    auto lr = Feature("MyDouble", 0.2);
    auto lc = Feature(MyDouble(0.2));
    auto rc = Feature(MyDouble(0.1));

    // (RuntimeFeature, Feature<T>) -> RuntimeAlgorithm
    auto ret1 = eval(f, lr, rc)->get();
    EXPECT_NEAR(ret1.value().get_as<double>("val"), 0.3, 1e-12);

    // (Feature<T>, Feature<T>) -> RuntimeAlgorithm
    auto ret2 = eval(f, lc, rc)->get();
    EXPECT_NEAR(ret2.value().get_as<double>("val"), 0.3, 1e-12);
}

TEST_F(RuntimeAlgorithmTest, MultiOutput)
{
    auto f = grunk::RuntimeFunction(&get_components);

    auto i = Feature("Point", 0.2, 0.6);
    auto fun = eval(f, i);
    auto x = fun->get<0>();
    auto y = fun->get<1>();

    EXPECT_EQ(Reflect::cast<double>(x.value()), 0.2);
    EXPECT_EQ(Reflect::cast<double>(y.value()), 0.6);

    i.access_value().set("y", 0.7);
    
    EXPECT_FALSE(x.is_valid());
    EXPECT_FALSE(y.is_valid());

    EXPECT_EQ(Reflect::cast<double>(y.value()), 0.7);
}

TEST_F(RuntimeAlgorithmTest, UnnamedFeature)
{
    auto f = RuntimeFunction(std::plus<double>());

    auto x = Feature("double", 0.7); // x is a named feature
    auto z = eval(f, x, 0.2)->get(); // 0.2 is an unnamed feature

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(Reflect::cast<double>(z.value()), 0.9, 1e-15);
    EXPECT_TRUE(z.is_valid());

    x.access_value() = 0.6;  // change named feature

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(Reflect::cast<double>(z.value()), 0.8, 1e-15);
    EXPECT_TRUE(z.is_valid());
}