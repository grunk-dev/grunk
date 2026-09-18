// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
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

    sol::object res1 = env["res1"];
    EXPECT_EQ(res1.as<Foo>().i, 12);

    sol::object res2 = env["res2"];
    EXPECT_EQ(res2.as<Foo>().i, 16);

    sol::object res3 = env["res3"];
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

namespace {

    struct Base
    {
        int i;
    };

    struct Derived : Base
    {
    };

} // anonymous namespace

// Regression test: Base.as_vec(...) must accept Derived-typed elements, not just
// exact Base ones - Derived is related to Base only via add_bases<Base>(), and
// sol::object::is<Base>()/.as<Base>() (what as_vec used before this fix) does not
// walk that inheritance chain the way an ordinary bound function's own parameter
// binding already correctly does for a T-typed argument. See with_std_vector's own
// doc comment for the sol2-internal reason (qualified_getter/type_cast vs. the
// generic is<T>()/as<T>() API).
TEST(std_vector, with_bases)
{
    grunk::state grunk;

    grunk.register_type<Base>("Base")
    .add_constructors([](int i){ return Base{i}; })
    .with_std_vector();

    grunk.register_type<Derived>("Derived")
    .add_constructors([](int i){ return Derived{Base{i}}; })
    .add_bases<Base>();

    auto env = grunk.create_env();
    env.eval(R"(
d1 = Derived.new(5)
d2 = Derived.new(7)
b1 = Base.new(9)

l = Base.as_vec(d1, d2, b1)
)");

    sol::object l = env["l"];
    ASSERT_TRUE(l.is<std::vector<Base>>());
    auto lv = l.as<std::vector<Base>>();
    ASSERT_EQ(lv.size(), 3u);
    EXPECT_EQ(lv[0].i, 5);
    EXPECT_EQ(lv[1].i, 7);
    EXPECT_EQ(lv[2].i, 9);
}

// with_std_vector()'s explicit VecElement template parameter (see its own doc
// comment) is meant to support std::vector<Handle<T>> for a
// sol::unique_usertype_traits-wrapped smart pointer around T too - the motivating
// case being OCCT's Handle(T)/opencascade::handle<T>, where a function expects
// std::vector<Handle(Base)> and Lua passes several Handle(Derived)-backed features.
// This is confirmed working end-to-end against real OCCT types in grunk-occt
// (Geom_Line/Geom_Geometry, both Handle-based with add_bases<...>() already
// registered): `Geom_Geometry.as_vec(line1, line2)` (two Handle(Geom_Line) values)
// correctly builds a std::vector<Handle(Geom_Geometry)>, and a function taking that
// vector reports the right size - see grunk-occt's tests/test_geom.cpp and
// docs/step5-investigation/ for the full writeup.
//
// A *self-contained* minimal repro of this specific case was attempted here (a
// small Handle<T> stand-in matching opencascade::handle<T>'s own shape, including
// its rebind_actual_type and upcast converting constructor) but did not reproduce
// the working behavior - it fails at sol2's own argument-checking step
// ("unrecognized userdata (not pushed by sol?)") even though the real OCCT case
// (structurally identical as far as could be determined: same automagic_flags,
// same add_bases usage, same registration order, tried both a plain registered
// constructor function and a real .add_constructors()-based one, tried a 2-level
// and a 3-level base chain) does not hit this failure. The exact discrepancy
// between the minimal stand-in and opencascade::handle<T> was not found despite a
// real debugging attempt (traced into sol2's stack_check_unqualified.hpp/
// stack_get_unqualified.hpp/inheritance.hpp - the regular, non-unique inheritance
// path is confirmed gated by weak_derive<T>, set correctly by add_bases; the
// unique-usertype path's own checker did not appear to consult it, in the
// stand-in's case). Filed as a follow-up rather than blocking this fix - see the
// grunk-occt writeup for the full trace. If you find the actual discrepancy, a
// `std_vector.with_bases_unique_usertype`-shaped test belongs here.