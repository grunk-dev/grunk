#include <gtest/gtest.h>

#include <grunk/dynamic/DynamicAction.hpp>

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

struct Counter {
    Counter(bool reset) {
        if (reset) {
            Counter::ctor = 0;
            Counter::copy = 0;
            Counter::copy_assignment = 0;
            Counter::move = 0;
            Counter::move_assignment = 0;
            Counter::dtor = 0;
        }

        ++Counter::ctor;
    };
    Counter(Counter const&) {
         ++Counter::copy;
    }
    Counter operator=(Counter const&) {
        ++Counter::copy_assignment;
        return *this;
    }
    Counter(Counter&&) {
        ++Counter::move;
    }
    Counter operator=(Counter&&) {
        ++Counter::move_assignment;
        return *this;
    }
    ~Counter() {
        ++Counter::dtor;
    }

    static int ctor;
    static int copy;
    static int copy_assignment;
    static int move;
    static int move_assignment;
    static int dtor;
};
int Counter::ctor = 0;
int Counter::copy = 0;
int Counter::copy_assignment = 0;
int Counter::move = 0;
int Counter::move_assignment = 0;
int Counter::dtor = 0;

int overloaded_function(double x) {
    return 0;
}

int overloaded_function(int x) {
    return 1;
}


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


class DynamicActionTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        reflect::register_type<double>("double")
        .add_constructor<double>();

        reflect::register_type<int>("int")
        .add_constructor<int>();

        reflect::register_type<MyDouble>("MyDouble")
        .add_constructor<double>()
        .add_data_member(&MyDouble::val, "val");

        reflect::register_type<Point>("Point")
        .add_constructor<double, double>()
        .add_data_member(&Point::x, "x")
        .add_data_member(&Point::y, "y");

        reflect::register_function<int(*)(double)>(&overloaded_function, "overloaded_function");
        reflect::register_function<int(*)(int)>(&overloaded_function, "overloaded_function");

        reflect::register_type<bool>("bool")
        .add_constructor<bool>();

        reflect::register_type<Counter>("Counter")
        .add_constructor<bool>();
    } 

    static void TearDownTestCase() {
        reflect::get_type_registry().clear();
    } 
};

TEST_F(DynamicActionTest, Basic)
{
    auto f = reflect::Callable(&add, "add");

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
    auto a = action("a", f, l, r).output();
    auto b = action("b", f, a, r).output();

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

TEST_F(DynamicActionTest, PassNonRumtimeFeatureToDynamicAction)
{
    auto f = reflect::Callable(&add, "add");

    auto lr = Feature("lr", "MyDouble", 0.2);
    auto lc = Feature("lc", MyDouble(0.2));
    auto rc = Feature("rc", MyDouble(0.1));

    // (DynamicFeature, Feature<T>) -> DynamicAction
    auto ret1 = action("ret1", f, lr, rc).output();
    EXPECT_NEAR(ret1.value().get_as<double>("val"), 0.3, 1e-12);

    // (Feature<T>, Feature<T>) -> DynamicAction
    auto ret2 = action("ret2", f, lc, rc).output();
    EXPECT_NEAR(ret2.value().get_as<double>("val"), 0.3, 1e-12);
}

TEST_F(DynamicActionTest, MultiOutput)
{
    auto f = reflect::Callable(&get_components, "get_components");

    auto i = Feature("i", "Point", 0.2, 0.6);
    auto o = action("o", f, i);
    auto x = o.output(0);
    auto y = o.output(1);

    // test default output feature ids
    EXPECT_EQ(x.id(), "o[0]");
    EXPECT_EQ(y.id(), "o[1]");

    EXPECT_EQ(x.get_type_descriptor()->get_name(), "double");
    EXPECT_EQ(y.get_type_descriptor()->get_name(), "double");

    // test renaming feature ids
    x.set_id("x");
    y.set_id("y");
    EXPECT_EQ(x.id(), "x");
    EXPECT_EQ(y.id(), "y");
    EXPECT_EQ(o.output(0).id(), "x");
    EXPECT_EQ(o.output(1).id(), "y");

    EXPECT_EQ(reflect::cast<double>(x.value()), 0.2);
    EXPECT_EQ(reflect::cast<double>(y.value()), 0.6);

    i.access_value().set("y", 0.7);
    
    EXPECT_FALSE(x.is_valid());
    EXPECT_FALSE(y.is_valid());

    EXPECT_EQ(reflect::cast<double>(y.value()), 0.7);
}

TEST_F(DynamicActionTest, UnnamedFeature)
{
    auto f = reflect::Callable(std::plus<double>(), "plus");

    auto x = Feature("x", "double", 0.7); // x is a named feature
    auto z = action("z", f, x, 0.2).output(); // 0.2 is an unnamed feature

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(reflect::cast<double>(z.value()), 0.9, 1e-15);
    EXPECT_TRUE(z.is_valid());

    x.access_value() = 0.6;  // change named feature

    EXPECT_FALSE(z.is_valid());
    EXPECT_NEAR(reflect::cast<double>(z.value()), 0.8, 1e-15);
    EXPECT_TRUE(z.is_valid());
}

TEST_F(DynamicActionTest, OverloadedFunction)
{
    {
        auto x = Feature("x", "double", 0.7);
        auto y = action("y", "overloaded_function", x).output().value();
        EXPECT_EQ(reflect::cast<int>(y), 0);
    }

    {
        auto x = Feature("x", "int", 7);
        auto y = action("y", "overloaded_function", x).output().value();
        EXPECT_EQ(reflect::cast<int>(y), 1);
    }
    
}

TEST_F(DynamicActionTest, ConstructorCall)
{
    // invoke constructor lazily via DynamicFeature, passing DynamicFeature to factory function
    {
        auto b = DynamicFeature("b", "bool", true);
        auto x = DynamicFeature::create("x", "Counter", b);
        EXPECT_EQ(Counter::ctor, 0);
        EXPECT_EQ(Counter::copy, 0);
        EXPECT_EQ(Counter::copy_assignment, 0);
        EXPECT_EQ(Counter::move, 0);
        EXPECT_EQ(Counter::move_assignment, 0);
        EXPECT_EQ(Counter::dtor, 0);

        reflect::DynamicObject y = x.value();
    }

    EXPECT_EQ(Counter::ctor, 1);
    EXPECT_EQ(Counter::copy, 0);
    EXPECT_EQ(Counter::copy_assignment, 0);
    EXPECT_EQ(Counter::move, 0);
    EXPECT_EQ(Counter::move_assignment, 0);
    EXPECT_EQ(Counter::dtor, 1);

    // invoke constructor lazily via grunk::action, passing DynamicFeature to factory function
    {
        auto b = DynamicFeature("b", "bool", true);
        auto x = grunk::action("x", "Counter", b).output();

        // until the ctor is called, these values will not be reset:
        EXPECT_EQ(Counter::ctor, 1);
        EXPECT_EQ(Counter::copy, 0);
        EXPECT_EQ(Counter::copy_assignment, 0);
        EXPECT_EQ(Counter::move, 0);
        EXPECT_EQ(Counter::move_assignment, 0);
        EXPECT_EQ(Counter::dtor, 1);

        reflect::DynamicObject y = x.value();
    }

    EXPECT_EQ(Counter::ctor, 1);
    EXPECT_EQ(Counter::copy, 0);
    EXPECT_EQ(Counter::copy_assignment, 0);
    EXPECT_EQ(Counter::move, 0);
    EXPECT_EQ(Counter::move_assignment, 0);
    EXPECT_EQ(Counter::dtor, 1);
}
