#include <gtest/gtest.h>

#include <grunk/dynamic/RuntimeFunction.h>

using namespace grunk;

namespace RuntimeFunction_test {

// a class with one const and one non-const member function
struct Foo {
    // const member function (should be allowed)
    std::string hello(int v) const
    {
        return "Hello from const function, input: " + std::to_string(v);
    }

    // non-const member function (should not be allowed)
    void set(int v) {
        val = v;
    }

    // const void member function
    void bar() const {}

    double operator()(double& input) const {
        return input * val;
    }

    int val {3};
};

// void function
void hello_world() {
    std::cout<<"Hello World\n";
}

// normal function
int fun0(double x) {
    return (int)x;
}

// function returning a tuple
std::tuple<int, double> fun1(std::string const& in){
    return std::make_tuple(4, 4.2);
}

} //namespace RuntimeFunction_test

using namespace RuntimeFunction_test;

class RuntimeFunctionTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {
        Reflect::Reflect<double>("double");
        Reflect::Reflect<int>("int");
        Reflect::Reflect<std::string>("string");

        Reflect::Reflect<Foo>("Foo")
        .AddDataMember(&Foo::val, "val")
        .AddMemberFunction(&Foo::hello, "hello")
        .AddMemberFunction(&Foo::set, "set")
        .AddMemberFunction(&Foo::bar, "bar");
    } 

    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};

TEST_F(RuntimeFunctionTest, voidFunction){
    auto f = RuntimeFunction(&hello_world);
    testing::internal::CaptureStdout();
    auto r = f();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Hello World\n");
    EXPECT_EQ(r.size(), 0);
}

TEST_F(RuntimeFunctionTest, FunctionPointer)
{
    auto f = RuntimeFunction(&fun0);
    auto x = Reflect::DynamicObject(2.2);
    auto r = f(x);
    EXPECT_EQ(r.size(), 1);
    EXPECT_EQ(Reflect::cast<int>(r[0]), 2);
}

TEST_F(RuntimeFunctionTest, FunctionReturningTuple)
{
    auto f = RuntimeFunction(&fun1);
    auto x = Reflect::DynamicObject(std::string("Hello"));
    auto r = f(x);
    EXPECT_EQ(r.size(), 2);
    EXPECT_EQ(Reflect::cast<int   >(r[0]), 4);
    EXPECT_EQ(Reflect::cast<double>(r[1]), 4.2);
}

TEST_F(RuntimeFunctionTest, ConstMemberFunction)
{
    auto f = RuntimeFunction(&Foo::hello);
    auto foo = Reflect::DynamicObject(Foo{});
    auto x = Reflect::DynamicObject(5);
    auto r = f(foo, x);
    EXPECT_EQ(r.size(), 1);
    EXPECT_EQ(Reflect::cast<std::string>(r[0]), "Hello from const function, input: 5");
}

TEST_F(RuntimeFunctionTest, ConstVoidMemberFunction)
{
    auto f = RuntimeFunction(&Foo::bar);
    auto foo = Reflect::DynamicObject(Foo{});
    auto r = f(foo);
    EXPECT_EQ(r.size(), 0);
}

TEST_F(RuntimeFunctionTest, NonConstMemberFunction)
{
    auto f = RuntimeFunction(&Foo::set);
    auto foo = Reflect::DynamicObject(Foo{});
    auto x = Reflect::DynamicObject(5);
    auto r = f(foo, x);
    EXPECT_EQ(r.size(), 0);

    EXPECT_EQ(foo.get_as<int>("val"), 5);
}

TEST_F(RuntimeFunctionTest, Lambda)
{
    bool proof = false;
    auto f = RuntimeFunction(
        [&proof](int i){ proof = true; return i*i; }
    );
    auto x = Reflect::DynamicObject(4);
    auto r = f(x);
    EXPECT_TRUE(proof);
    EXPECT_EQ(r.size(), 1);
    EXPECT_EQ(Reflect::cast<int>(r[0]), 16); // no rounding with power of two
}

TEST_F(RuntimeFunctionTest, MutableLambda)
{
    int j = 0;
    auto f = RuntimeFunction(
        [=](int i) mutable {  j=4; return j*i; }
    );
    auto x = Reflect::DynamicObject(4);
    auto r = f(x);
    EXPECT_EQ(j, 0);
    EXPECT_EQ(r.size(), 1);
    EXPECT_EQ(Reflect::cast<int>(r[0]), 16); // no rounding with power of two
}

TEST_F(RuntimeFunctionTest, StdFunction)
{
    auto f = RuntimeFunction(std::function(&fun0));
    auto x = Reflect::DynamicObject(2.2);
    auto r = f(x);
    EXPECT_EQ(r.size(), 1);
    EXPECT_EQ(Reflect::cast<int>(r[0]), 2);
}

TEST_F(RuntimeFunctionTest, CallOperator)
{
    auto f = RuntimeFunction(Foo());
    auto x = Reflect::DynamicObject(1.1);
    auto r = f(x);
    EXPECT_EQ(r.size(), 1);
    EXPECT_NEAR(Reflect::cast<double>(r[0]), 3.3, 1e-10);
}