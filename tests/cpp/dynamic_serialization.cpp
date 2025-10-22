#include <gtest/gtest.h>
#include <grunk/dynamic/state.hpp>

TEST(serialization, primitives)
{
    grunk::state grunk;

    { // double
        auto d = grunk.feature(2.234);
        auto ret = d.node_pointer()->serialize();
        ASSERT_EQ(ret, "2.234");

        auto o = grunk.deserialize(ret);
        EXPECT_TRUE(o.is<double>());
        EXPECT_EQ(o, d.value()); // not testing for floating point eq. Exact round-tripping would be ideal
        EXPECT_EQ(o.as<double>(), d.value().as<double>());
    }

    { // int
        auto i = grunk.feature(2);
        auto ret = i.node_pointer()->serialize();
        ASSERT_EQ(ret, "2");

        auto o = grunk.deserialize(ret);
        EXPECT_TRUE(o.is<int>());
        EXPECT_EQ(o, i.value());
        EXPECT_EQ(o.as<int>(), i.value().as<int>());
    }

    { // string
        auto s = grunk.feature("Hello World");
        auto ret = s.node_pointer()->serialize();
        ASSERT_EQ(ret, "\"Hello World\"");

        auto o = grunk.deserialize(ret);
        EXPECT_TRUE(o.is<std::string>());
        EXPECT_EQ(o, s.value());
        EXPECT_EQ(o.as<std::string>(), s.value().as<std::string>());
    }

    { // boolean
        auto b = grunk.feature(true);
        auto ret = b.node_pointer()->serialize();
        EXPECT_EQ(ret, "true");

        auto o = grunk.deserialize(ret);
        EXPECT_TRUE(o.is<bool>());
        EXPECT_EQ(o, b.value());
        EXPECT_EQ(o.as<bool>(), b.value().as<bool>());

        b.set_value(false);
        ret = b.node_pointer()->serialize();
        EXPECT_EQ(ret, "false");

        o = grunk.deserialize(ret);
        EXPECT_TRUE(o.is<bool>());
        EXPECT_EQ(o, b.value());
        EXPECT_EQ(o.as<bool>(), b.value().as<bool>());
    }

    { // nil
        auto n = grunk.feature(sol::nil);
        auto ret = n.node_pointer()->serialize();
        EXPECT_EQ(ret, "nil");

        auto o = grunk.deserialize(ret);
        EXPECT_TRUE(o.is<sol::nil_t>());
        EXPECT_EQ(o, n.value());
    }
}

namespace {

struct Foo {

    Foo(int i, std::string_view s)
    : bar{i}
    , baz{s}
    {};

    int bar;
    std::string baz;
};

} // anonymous namespace

TEST(serialization, userdata)
{
    grunk::state grunk;
    grunk.set_functions_are_actions(false);

    grunk.register_type<Foo>("Foo")
    .add_constructors<Foo(int, std::string_view)>()
    .add_member_function("serialize",
        [](Foo const& foo) {
            return "Foo.new(" + std::to_string(foo.bar) + ", " + "\"" + foo.baz + "\"" + ")";
        }
    );
    
    auto foo = grunk.feature(Foo(15, "bazinga"));
    auto ret = foo.node_pointer()->serialize();
    ASSERT_EQ(ret, "Foo.new(15, \"bazinga\")");


    auto o = grunk.deserialize(ret);
    EXPECT_TRUE(o.is<Foo>());
    // EXPECT_EQ(o, foo.value());  // This does not seem to be a given for usertype

    Foo lhs = foo.value().as<Foo>();
    Foo rhs = o.as<Foo>();
    EXPECT_EQ(lhs.bar, rhs.bar);
    EXPECT_EQ(lhs.baz, rhs.baz);
}

TEST(serialization, free_function_action)
{
    grunk::state grunk;
    grunk.register_function("add", [](double l, double r) {
        return l + r;
    });

    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = grunk.action("add", x, y).with_id("z");

    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = add(x, y)");
}

TEST(serialization, free_function_action_anonymous1)
{ 
    grunk::state grunk;
    grunk.register_function("add", [](double l, double r) {
        return l + r;
    });

    // anonymous x
    auto x = grunk.feature(2.);
    auto y = grunk.feature(3.).with_id("y");
    auto z = grunk.action("add", x, y).with_id("z");

    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = add(2, y)");
}

TEST(serialization, free_function_action_anonymous2)
{ 
    grunk::state grunk;
    grunk.register_function("add", [](double l, double r) {
        return l + r;
    });

    // anonymous y
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.);
    auto z = grunk.action("add", x, y).with_id("z");

    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = add(x, 3)");
}

TEST(serialization, free_function_action_anonymous3)
{ 
    grunk::state grunk;
    grunk.register_function("add", [](double l, double r) {
        return l + r;
    });

    // anonymous x and y
    auto x = grunk.feature(2.);
    auto y = grunk.feature(3.);
    auto z = grunk.action("add", x, y).with_id("z");

    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = add(2, 3)");
}

TEST(serialization, free_function_action_anonymous4)
{ 
    grunk::state grunk;
    grunk.register_function("add", [](double l, double r) {
        return l + r;
    });
    
    // anonymous z
    auto x = grunk.feature(2.);
    auto y = grunk.feature(3.);
    auto z = grunk.action("add", x, y);

    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "add(2, 3)");
}

TEST(serialization, free_function_action_anonymous_nested)
{ 
    grunk::state grunk;
    grunk.register_function("add", [](double l, double r) {
        return l + r;
    });

    // nested anonymous features
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.);
    auto z = grunk.action("add", x, y);
    auto w = grunk.action("add", z, 5).with_id("w");

    auto ret = w.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "w = add(add(x, 3), 5)");
}

TEST(serialization, operator_action_lua_add)
{
    grunk::state grunk;

    grunk.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x + y 
    )");
    auto z = grunk.get_feature("z");

    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x + y");
}

TEST(serialization, operator_action_cpp_add)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x + y;
    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x + y");
}

TEST(serialization, operator_action_cpp_add_anonymous1)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x + y;
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "(x + y)");
}

TEST(serialization, operator_action_cpp_add_anonymous2)
{
    grunk::state grunk;
    auto x = grunk.feature(2.);
    auto y = grunk.feature(3.).with_id("y");
    auto z = x + y;
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "(2 + y)");
}

TEST(serialization, operator_action_cpp_add_anonymous_nested)
{
    grunk::state grunk;
    auto x = grunk.feature(2.);
    auto y = grunk.feature(3.).with_id("y");
    auto z = x + y;
    auto w = z + 5;
    w.set_id("w");
    auto ret = w.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "w = (2 + y) + 5");
}

TEST(serialization, operator_action_lua_sub)
{
    grunk::state grunk;

    grunk.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x - y
    )");
    auto z = grunk.get_feature("z");

    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x - y");
}

TEST(serialization, operator_action_cpp_sub)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x - y;
    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x - y");
}

TEST(serialization, operator_action_lua_mul)
{
    grunk::state grunk;

    grunk.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x * y
    )");
    auto z = grunk.get_feature("z");

    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x * y");
}

TEST(serialization, operator_action_cpp_mul)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x * y;
    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x * y");
}

TEST(serialization, operator_action_lua_div)
{
    grunk::state grunk;

    grunk.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x / y
    )");
    auto z = grunk.get_feature("z");

    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x / y");
}

TEST(serialization, operator_action_cpp_div)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x / y;
    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x / y");
}

TEST(serialization, operator_action_lua_pow)
{
    grunk::state grunk;

    grunk.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x ^ y
    )");
    auto z = grunk.get_feature("z");

    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x ^ y");
}

TEST(serialization, operator_action_cpp_pow)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = grunk::pow(x,y);
    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x ^ y");
}

TEST(serialization, operator_action_lua_mod)
{
    grunk::state grunk;

    grunk.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x % y 
    )");
    auto z = grunk.get_feature("z");

    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x % y");
}

TEST(serialization, operator_action_cpp_mod)
{
    grunk::state grunk;
    auto x = grunk.feature(2).with_id("x");
    auto y = grunk.feature(3).with_id("y");
    auto z = x % y;
    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = x % y");
}

TEST(serialization, operator_action_lua_unm)
{
    grunk::state grunk;

    grunk.eval(R"(
        x = grunk.feature(2.):with_id("x")
        z = -x
    )");
    auto z = grunk.get_feature("z");

    z.set_id("z");
    EXPECT_EQ(z.node_pointer()->compute_node()->get_parents().size(), 1);
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = -x");
}

TEST(serialization, operator_action_cpp_unm)
{
    grunk::state grunk;
    auto x = grunk.feature(2).with_id("x");
    auto z = -x;
    z.set_id("z");
    auto ret = z.node_pointer()->compute_node()->serialize();
    EXPECT_EQ(ret, "z = -x");
}