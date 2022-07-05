#include <gtest/gtest.h>

#include <grunk/RuntimeObject.h>

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
    EXPECT_EQ(x.Get("value").cast<double>(), 0.5);

    // ctor called with unregistered type, e.g. bool
    EXPECT_THROW(RuntimeObject(true), std::domain_error);
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

    EXPECT_THROW(x.Get("non existentent member"), std::invalid_argument);
    EXPECT_THROW(x.Set("value", std::string("wrong argument type")), std::invalid_argument);
    EXPECT_THROW(x.Set("non existent member", 123), std::invalid_argument);
}

TEST_F(RuntimeObjectTest, InvokeNonConst)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    x.Invoke("multiply", 2);
    EXPECT_NEAR(x.Get("value").cast<double>(), 1., 1e-10);

    RuntimeObject z(3);
    x.Invoke("multiply", z);
    EXPECT_NEAR(x.Get("value").cast<double>(), 3., 1e-10);

    EXPECT_THROW(x.Invoke("nonexistent member function"), std::invalid_argument);
}

TEST_F(RuntimeObjectTest, InvokeConstCorrectness)
{
    RuntimeObject x = make_rto("MyDouble", 0.5);
    RuntimeObject const& xcref = x;
    RuntimeObject& xref = x;

    // calling const member fun on const ref
    // ASSERT_NO_THROW(xcref.Invoke("get"));
    // auto r = xcref.Invoke("get");
    // EXPECT_EQ(r.cast<double>(), 0.5);

    // calling nonconst member fun on const ref
    // auto r = xcref.Invoke("multiply", 3);
    // TODO: ThIS SHOULD THROW disregards_qualifier, derived from std::exception

    // calling const member on ref
    EXPECT_NO_THROW(xref.Invoke("get"));

    // calling nonconst member on ref
    EXPECT_NO_THROW(xref.Invoke("multiply", 5));
    EXPECT_NEAR(x.Get("value").cast<double>(), 2.5, 1e-12);
}

TEST_F(RuntimeObjectTest, make_rto)
{
    auto x = make_rto("MyDouble", 0.5);
    EXPECT_EQ(x.Get("value").cast<double>(), 0.5);

    EXPECT_THROW(make_rto("NonRegisteredType", 0.5), std::invalid_argument);

    EXPECT_THROW(make_rto("MyDouble", 0.5, 0.5, 0.5), std::invalid_argument);
}

// TODO: Test misuse and error handling (To Do: error handling ;) )

