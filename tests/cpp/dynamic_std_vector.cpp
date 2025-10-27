#include <gtest/gtest.h>
#include <grunk/dynamic/state.hpp>
#include <numeric>

namespace {

    struct Foo
    {
        int i;
    };

    Foo add(std::vector<Foo> const& v) {
        return std::accumulate(v.begin(), v.end(), Foo{0}, [](auto const& l, auto const& r){ return Foo{l.i + r.i}; });
    }

} // anonymous namespace

TEST(std_vector, no_action)
{
    grunk::state grunk;
    
    grunk.register_type<Foo>("Foo")
    .add_constructors([](int i){ return Foo{i}; })
    .with_std_vector();

    grunk.register_function("add", &add);

    auto env = grunk.create_env();

    env.eval(R"(
x1 = Foo.new(5)
x2 = Foo.new(7)
x3 = Foo.new(9)

l1 = Foo.as_vec(x1, x2)
res1 = add(l1)

l2 = Foo.as_vec({x2, x3})
res2 = add(l2)

l2:add(x1)
res3 = add(l2)
)");

    auto res1 = env["res1"];
    EXPECT_EQ(res1.as<Foo>().i, 12);

    auto res2 = env["res2"];
    EXPECT_EQ(res2.as<Foo>().i, 16);

    auto res3 = env["res3"];
    EXPECT_EQ(res3.as<Foo>().i, 21);
}

TEST(std_vector, no_action_exceptions)
{
    grunk::state grunk;
    
    grunk.register_type<Foo>("Foo")
    .add_constructors([](int i){ return Foo{i}; })
    .with_std_vector();

    // cant test in LUA script:
    // https://github.com/ThePhD/sol2/issues/1724
    auto as_vec =  grunk.get_function("Foo.as_vec");

    auto env = grunk.create_env();

    env.eval(R"(t = {2.3, "horst"})");
    auto t = env["t"];
    auto ret1 = as_vec.call(t);
    ASSERT_FALSE(ret1.valid());

    auto ret2 = as_vec.call(4, "wrong");
    ASSERT_FALSE(ret2.valid());
}

TEST(std_vector, as_action_lua)
{
    grunk::state grunk;
    
    grunk.register_type<Foo>("Foo")
    .add_constructors([](int i){ return Foo{i}; })
    .with_std_vector();

    grunk.register_function("add", &add);

    auto env = grunk.create_parametric_env();
    env.eval(R"(
x1 = Foo.new_feature(5)
x2 = Foo.new_feature(7)
x3 = Foo.new_feature(9)

l = Foo.as_vec(x1, x2, x3)
res = add(Foo.as_vec(x1, x2, x3))
)");

    auto l = env.get_feature("l");
    ASSERT_TRUE(l.value().is<std::vector<Foo>>());
    auto lv = l.value().as<std::vector<Foo>>();
    ASSERT_EQ(lv.size(), 3);
    EXPECT_EQ(lv[0].i, 5);
    EXPECT_EQ(lv[1].i, 7);
    EXPECT_EQ(lv[2].i, 9);

    auto res = env.get_feature("res");
    EXPECT_EQ(res.value().as<Foo>().i, 21);
    auto x1 = env.get_feature("x1");
    x1.set_value(Foo{26});
    EXPECT_EQ(res.value().as<Foo>().i, 42);
}

TEST(std_vector, as_action_cpp)
{
    grunk::state grunk;
    
    grunk.register_type<Foo>("Foo")
    .add_constructors([](int i){ return Foo{i}; })
    .with_std_vector();

    grunk.register_function("add", &add);

    auto x = grunk.feature("Foo", 5);
    auto y = grunk.feature("Foo", 7);
    auto z = grunk.feature("Foo", 9);
    auto l = grunk.action("Foo.as_vec", x, y, z);
    auto res = grunk.action("add", l);

    ASSERT_TRUE(l.value().is<std::vector<Foo>>());
    auto lv = l.value().as<std::vector<Foo>>();
    ASSERT_EQ(lv.size(), 3);
    EXPECT_EQ(lv[0].i, 5);
    EXPECT_EQ(lv[1].i, 7);
    EXPECT_EQ(lv[2].i, 9);

    EXPECT_EQ(res.value().as<Foo>().i, 21);
    x.set_value(Foo{26});
    EXPECT_EQ(res.value().as<Foo>().i, 42);
}