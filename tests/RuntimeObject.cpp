#include <gtest/gtest.h>

#include <grunk/RuntimeObject.h>

using namespace grunk;

struct MyDouble {

    MyDouble(double v) : value(v) {}

    void multiply(int factor) {
        value *= factor;
    }

    double value;
};

class RuntimeObjectTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {
        Reflect::Reflect<double>("double");

        Reflect::Reflect<int>("int");

        Reflect::Reflect<MyDouble>("MyDouble")
        .AddConstructor<double>()
        .AddMemberFunction(&MyDouble::multiply, "multiply")
        .AddDataMember(&MyDouble::value, "value");
    } 
    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};

TEST_F(RuntimeObjectTest, CtorSimple)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    EXPECT_EQ(x.Get("value").cast<double>(), 0.5);
}

TEST_F(RuntimeObjectTest, Casting)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    
    auto* y = x.cast<MyDouble*>();
    y->value = 13.;
    EXPECT_EQ(x.Get("value").cast<double>(), 13.);

    auto& z = x.cast<MyDouble&>();
    z.value = 11.1;
    EXPECT_EQ(x.Get("value").cast<double>(), 11.1);

    auto cpy = x.cast<MyDouble>();
    cpy.value = 0.124;
    EXPECT_EQ(x.Get("value").cast<double>(), 11.1);
}

TEST_F(RuntimeObjectTest, SetterGetter)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    
    RuntimeObject y(0.25);
    x.Set("value", y);
    EXPECT_EQ(x.Get("value").cast<double>(), 0.25);

    // setting and getting by conversion with other types
    x.Set("value", 3.14);
    EXPECT_EQ(x.Get("value").cast<double>(), 3.14);
}

TEST_F(RuntimeObjectTest, Invoke)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    x.Invoke("multiply", 2);
    EXPECT_NEAR(x.Get("value").cast<double>(), 1., 1e-10);

    RuntimeObject z(3);
    x.Invoke("multiply", z);
    EXPECT_NEAR(x.Get("value").cast<double>(), 3., 1e-10);
}

// TODO: Test misuse and error handling (To Do: error handling ;) )

