#include <gtest/gtest.h>

#include <grunk/grunk.hpp>

using namespace grunk;

namespace {

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

} //namespace

TEST(ActionTest, Basic)
{
    // l and r are the root input nodes
    auto l = Feature("l", MyDouble(0.2));
    auto r = Feature("r", MyDouble(0.1));

    // a depends on l and r, b depends on a and r
    //    
    //   l      r
    //    \   / |
    //      a   |
    //       \  |
    //         b
    //
    auto a = action("a", &add, l, r)->get();
    auto b = action("b", &add, a, r)->get();

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


TEST(ActionTest, MultiOutput)
{
    auto f = &get_components;

    auto i = Feature("i", Point(0.2, 0.6));
    auto x = action("x", f, i)->get<0>();
    auto y = action("y", f, i)->get<1>();

    EXPECT_EQ(x.value(), 0.2);
    EXPECT_EQ(y.value(), 0.6);

    i.access_value().y = 0.7;
    
    EXPECT_FALSE(x.is_valid());
    EXPECT_FALSE(y.is_valid());

    EXPECT_EQ(y.value(), 0.7);
}

TEST(ActionTest, UnnamedFeature)
{
    auto x = Feature("x", 0.7);                     // x is a named feature
    auto z = action("z", std::plus<>{}, x, 0.2)->get(); // 0.2 is an unnamed feature

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(z.value(), 0.9, 1e-15);
    EXPECT_TRUE(z.is_valid());

    x.access_value() = 0.6;  // change named feature

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(z.value(), 0.8, 1e-15);
    EXPECT_TRUE(z.is_valid());
}
