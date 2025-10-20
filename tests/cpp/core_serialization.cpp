#include <gtest/gtest.h>
#include <grunk/core/state.hpp>

TEST(io, primitives)
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

TEST(io, userdata)
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