#include <gtest/gtest.h>
#include <grunk/grunk.hpp>

using namespace grunk;

namespace {

static bool f_called = false;

double f(double in) {
    f_called = true;
    return in*in;
}

double h(double in)
{
    EXPECT_TRUE(f_called);
    return in/2.;
}

double h_meta(Feature<double> const& in)
{
    EXPECT_FALSE(f_called);
    auto ret = in.value()/2.;
    EXPECT_TRUE(f_called);
    return ret;
}

} // namespace

class MetaTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        // reflect::register_type<DynamicFeature>("Feature")
        // .add_constructor<int>();

        // reflect::register_function(
        //     [](double const& l, double const& r){ return l+r;}, 
        //     "plus"
        // );
    } 

    static void TearDownTestCase() {
    } 

};


TEST_F(MetaTest, static_mode)
{
    {
        // calling x->f->h in the normal way. 
        // f should be called when entering the function body of h
        f_called = false;
        auto x = Feature("x", 0.5);
        auto y = action("y", &f, x)->output();
        auto z = action("z", &h, y)->output();
        ASSERT_NEAR(z.value(), 0.125, 1e-15);
    }
    {
        // calling x->f->wrap-in-feature->h_meta. h_meta accepts a feature node and can
        // postpone the evaluation of the feature tree.
        f_called = false;
        auto x = Feature("x", 0.5);
        auto y = action("y", &f, x)->output();
        auto fy = Feature("fy", std::move(y)); //to do: make parametric accept const references
        auto z = action("z", &h_meta, fy)->output();
        ASSERT_NEAR(z.value(), 0.125, 1e-15);
    }
}