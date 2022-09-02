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

TEST_F(RuntimeAlgorithmTest, Basic)
{
    auto f = Reflect::Function(&add, "add");

    // l and r are the root input nodes
    auto l = Feature("l", "MyDouble", 0.2);
    auto r = Feature("r", "MyDouble", 0.1);

    // a depends on l and r, b depends on a and r
    //    
    //   l      r
    //    \   / |
    //      a   |
    //       \  |
    //         b
    //
    auto a = eval("a", f, l, r)->get();
    auto b = eval("b", f, a, r)->get();

    // nothing has been computed yet, we just registered the feature tree
    EXPECT_FALSE(a.is_valid());
    EXPECT_FALSE(b.is_valid());
    
    EXPECT_EQ(a.id(), "a");
    EXPECT_EQ(b.id(), "b");

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
    auto f = Reflect::Function(&add, "add");

    auto lr = Feature("lr", "MyDouble", 0.2);
    auto lc = Feature("lc", MyDouble(0.2));
    auto rc = Feature("rc", MyDouble(0.1));

    // (RuntimeFeature, Feature<T>) -> RuntimeAlgorithm
    auto ret1 = eval("ret1", f, lr, rc)->get();
    EXPECT_NEAR(ret1.value().get_as<double>("val"), 0.3, 1e-12);

    // (Feature<T>, Feature<T>) -> RuntimeAlgorithm
    auto ret2 = eval("ret2", f, lc, rc)->get();
    EXPECT_NEAR(ret2.value().get_as<double>("val"), 0.3, 1e-12);
}

TEST_F(RuntimeAlgorithmTest, MultiOutput)
{
    auto f = Reflect::Function(&get_components, "get_components");

    auto i = Feature("i", "Point", 0.2, 0.6);
    auto o = eval("o", f, i);
    auto x = o->get<0>();
    auto y = o->get<1>();

    // test default output feature ids
    EXPECT_EQ(x.id(), "o::0");
    EXPECT_EQ(y.id(), "o::1");

    // test renaming feature ids
    x.set_id("x");
    y.set_id("y");
    EXPECT_EQ(x.id(), "x");
    EXPECT_EQ(y.id(), "y");
    EXPECT_EQ(o->get<0>().id(), "x");
    EXPECT_EQ(o->get<1>().id(), "y");

    EXPECT_EQ(Reflect::cast<double>(x.value()), 0.2);
    EXPECT_EQ(Reflect::cast<double>(y.value()), 0.6);

    i.access_value().set("y", 0.7);
    
    EXPECT_FALSE(x.is_valid());
    EXPECT_FALSE(y.is_valid());

    EXPECT_EQ(Reflect::cast<double>(y.value()), 0.7);
}