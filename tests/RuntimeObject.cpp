#include <gtest/gtest.h>

#include <grunk/runtime/RuntimeObject.h>

using namespace grunk;

struct MyDouble {

    MyDouble(double v) : value(v) {}

    void multiply(int factor) {
        value *= factor;
    }

    double get() const {
        return value;
    }

    double value;
};

class RuntimeObjectTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {

        Reflect::Reflect<std::string>("string");

        Reflect::Reflect<double>("double");

        Reflect::Reflect<int>("int");

        Reflect::Reflect<MyDouble>("MyDouble")
        .AddConstructor<double>()
        .AddMemberFunction(&MyDouble::multiply, "multiply")
        .AddMemberFunction(&MyDouble::get, "get")
        .AddDataMember(&MyDouble::value, "value");
    } 
    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};

TEST_F(RuntimeObjectTest, CtorSimple)
{
    RuntimeObject x(MyDouble(0.5));
    EXPECT_EQ(x.get("value").cast<double>(), 0.5);

    // ctor called with unregistered type, e.g. bool
    EXPECT_THROW(RuntimeObject(true), std::domain_error);
}

TEST_F(RuntimeObjectTest, Casting)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    
    auto* y = x.cast<MyDouble*>();
    y->value = 13.;
    EXPECT_EQ(x.get("value").cast<double>(), 13.);

    auto& z = x.cast<MyDouble&>();
    z.value = 11.1;
    EXPECT_EQ(x.get("value").cast<double>(), 11.1);

    auto cpy = x.cast<MyDouble>();
    cpy.value = 0.124;
    EXPECT_EQ(x.get("value").cast<double>(), 11.1);
}

TEST_F(RuntimeObjectTest, SetterGetter)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    
    RuntimeObject y(0.25);
    x.set("value", y);
    EXPECT_EQ(x.get("value").cast<double>(), 0.25);

    // setting and getting by conversion with other types
    x.set("value", 3.14);
    EXPECT_EQ(x.get("value").cast<double>(), 3.14);

    EXPECT_THROW(x.get("non existentent member"), std::invalid_argument);
    EXPECT_THROW(x.set("value", std::string("wrong argument type")), std::invalid_argument);
    EXPECT_THROW(x.set("non existent member", 123), std::invalid_argument);
}

TEST_F(RuntimeObjectTest, InvokeNonConst)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    x.invoke("multiply", 2);
    EXPECT_NEAR(x.get("value").cast<double>(), 1., 1e-10);

    RuntimeObject z(3);
    x.invoke("multiply", z);
    EXPECT_NEAR(x.get("value").cast<double>(), 3., 1e-10);

    EXPECT_THROW(x.invoke("nonexistent member function"), std::invalid_argument);
}

TEST_F(RuntimeObjectTest, InvokeConstCorrectness)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    RuntimeObject const& xcref = x;
    RuntimeObject& xref = x;

    // calling const member fun on const ref
    ASSERT_NO_THROW(xcref.invoke("get"));
    auto r = xcref.invoke("get");
    EXPECT_EQ(r.cast<double>(), 0.5);

    // calling nonconst member fun on const ref
    EXPECT_THROW(xcref.invoke("multiply", 3), Reflect::disregards_qualifier);

    // calling const member on ref
    EXPECT_NO_THROW(xref.invoke("get"));

    // calling nonconst member on ref
    EXPECT_NO_THROW(xref.invoke("multiply", 5));
    EXPECT_NEAR(x.get("value").cast<double>(), 2.5, 1e-12);
}

TEST_F(RuntimeObjectTest, make_rto)
{
    auto x = make_rto("MyDouble", 0.5);
    EXPECT_EQ(x.get("value").cast<double>(), 0.5);

    EXPECT_THROW(make_rto("NonRegisteredType", 0.5), std::invalid_argument);

    EXPECT_THROW(make_rto("MyDouble", 0.5, 0.5, 0.5), std::invalid_argument);
}

