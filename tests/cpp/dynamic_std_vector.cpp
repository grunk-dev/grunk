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

    grunk.set_functions_are_actions(false);

    grunk.eval(R"(
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

    auto res1 = grunk["res1"];
    EXPECT_EQ(res1.as<Foo>().i, 12);

    auto res2 = grunk["res2"];
    EXPECT_EQ(res2.as<Foo>().i, 16);

    auto res3 = grunk["res3"];
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

    grunk.eval(R"(t = {2.3, "horst"})");
    auto t = grunk["t"];
    auto ret1 = as_vec.call(t);
    ASSERT_FALSE(ret1.valid());

    auto ret2 = as_vec.call(4, "wrong");
    ASSERT_FALSE(ret2.valid());
}

TEST(std_vector, lua)
{
    grunk::state grunk;
    
    grunk.register_type<Foo>("Foo")
    .add_constructors([](int i){ return Foo{i}; })
    .with_std_vector();

    grunk.register_function("add", &add);

    grunk.eval(R"(
x1 = Foo.new_feature(5)
x2 = Foo.new_feature(7)
x3 = Foo.new_feature(9)

l = Foo.as_vec(x1, x2, x3)
-- res = add(Foo.as_vec(x1, x2, x3))
)");

    auto l = grunk.get_feature("l");
    ASSERT_TRUE(l.value().is<std::vector<Foo>>());
    auto lv = l.value().as<std::vector<Foo>>();
    ASSERT_EQ(lv.size(), 3);
    EXPECT_EQ(lv[0].i, 5);
    EXPECT_EQ(lv[1].i, 7);
    EXPECT_EQ(lv[2].i, 9);

    // auto res = grunk.get_feature("res");
    // EXPECT_EQ(res.value().as<Foo>().i, 21);
}