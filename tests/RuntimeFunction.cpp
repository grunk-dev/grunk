#include <gtest/gtest.h>

#include <grunk/RuntimeFunction.h>

using namespace grunk;

class RuntimeFunctionTest : public ::testing::Test 
{
public:

    static void SetUpTestCase() {
        Reflect::Reflect<double>("double");
        Reflect::Reflect<int>("int");
        Reflect::Reflect<std::string>("string");
    } 
    static void TearDownTestCase() {
        Reflect::GetTypeRegistry().clear();
    } 
};


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

    int val {3};
};


// normal function
int fun0(double x) {
    return (int)x;
}

// function returning a tuple
std::tuple<int, double> fun1(std::string const& in){
    return std::make_tuple(4, 4.2);
}

TEST_F(RuntimeFunctionTest, FunctionPointer)
{
    auto f = RuntimeFunction(&fun0);
    auto x = RuntimeObject(2.2);
    auto r = f(x);
    EXPECT_EQ(r.size(), 1);
    EXPECT_EQ(r[0].cast<int>(), 2);
}

TEST_F(RuntimeFunctionTest, FunctionReturningTuple)
{
    auto f = RuntimeFunction(&fun1);
    auto x = RuntimeObject(std::string("Hello"));
    auto r = f(x);
    EXPECT_EQ(r.size(), 2);
    EXPECT_EQ(r[0].cast<int>(), 4);
    EXPECT_EQ(r[1].cast<double>(), 4.2);
}

TEST_F(RuntimeFunctionTest, ConstMemberFunction)
{
    auto f = RuntimeFunction(&Foo::hello);
    auto foo = RuntimeObject(Foo{});
    auto x = RuntimeObject(5);
    auto r = f(foo, x);
    EXPECT_EQ(r.size(), 1);
    EXPECT_EQ(r[0].cast<std::string>(), "Hello from const function, input: 5");
}

// TEST_F(RuntimeFunctionTest, ConstVoidMemberFunction)
// {
//     auto f = RuntimeFunction(&Foo::bar);
//     auto foo = RuntimeObject(Foo{});
//     auto r = f(foo);
//     EXPECT_EQ(r.size(), 0);
// }

// TEST_F(RuntimeFunctionTest, NonConstMemberFunction)
// {
//     auto f = RuntimeFunction(&Foo::set);
//     auto foo = Foo{};
//     auto foo_rto = RuntimeObject(&foo);
//     auto x = RuntimeObject(5);
//     auto r = f(foo_rto, x);
//     EXPECT_EQ(r.size(), 0);

//     EXPECT_EQ(foo.val, 5);
// }

// TODO: Test function object with operator, lambda, member functions, function returning tuple