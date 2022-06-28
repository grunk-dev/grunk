#include <gtest/gtest.h>

#include <grunk/FunctionRegistry.h>

using namespace grunk;

namespace FunctionRegistry_test {

// a class with one const and one non-const member function
struct MyDouble {

    MyDouble(double v) : val{v} {}

    double val;
};


// normal function
MyDouble add(MyDouble const& l, MyDouble const& r) {
    return MyDouble(l.val + r.val);
}

} //namespace FunctionRegistry_test

using namespace FunctionRegistry_test;

class FunctionRegistryTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {
        Reflect::Reflect<double>("double");

        Reflect::Reflect<MyDouble>("MyDouble")
        .AddConstructor<double>()
        .AddDataMember(&MyDouble::val, "val");

        grunk::register_function(&add, "add");
    } 

    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
        grunk::get_function_registry().clear();
    } 
};

TEST_F(FunctionRegistryTest, BasicUsage)
{
    Feature l("MyDouble", 3.3);
    Feature r("MyDouble", 2.2);

    auto a = eval("add", l, r)->get();
    auto b = eval("add", a, r)->get();

    EXPECT_FALSE(a.is_valid());
    EXPECT_FALSE(b.is_valid());

    EXPECT_NEAR(b.Value().Get("val").cast<double>(), 7.7, 1e-12);

    EXPECT_TRUE(a.is_valid());
    EXPECT_TRUE(b.is_valid());
}