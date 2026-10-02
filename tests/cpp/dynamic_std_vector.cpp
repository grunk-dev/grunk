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
//
// A prior version of this fix claimed this was "confirmed working end-to-end
// against real OCCT types in grunk-occt" but a self-contained minimal Handle<T>
// stand-in reproduced a failure ("unrecognized userdata (not pushed by sol?)")
// that could not be explained at the time. Root-caused for real since then, against
// grunk-geoml's own real OCCT Handle(Geom_Curve)/Handle(Geom_BSplineCurve) case
// (occt.Geom_Curve.as_vec(...) previously failed identically) - two independent,
// jointly-necessary causes, both fixed/documented now:
//
// 1. cast_to_element's parameter was VecElement const& (a reference). sol2's
//    unique-usertype-aware checker/getter (the path that actually consults
//    rebind_actual_type/SOL_BASE_CLASSES to walk a Handle(Derived)->Handle(Base)
//    conversion) excludes reference parameters outright - the identical
//    !std::is_reference_v<X> gate that already forced grunk-occt/grunk-geoml's own
//    codegen to emit Handle(X) function parameters by value. Fixed above:
//    cast_to_element now takes VecElement by value.
// 2. SOL_BASE_CLASSES(T, ...)/SOL_DERIVED_CLASSES(T, ...) (sol/forward.hpp)
//    specialize the compile-time sol::base<T>/sol::derive<T> traits - visible only
//    via #include, per ordinary C++ template-specialization rules, independent of
//    whatever add_bases<...>() registered at runtime. The original stand-in
//    attempt registered add_bases<Base>() (the runtime side) but never declared
//    SOL_BASE_CLASSES/SOL_DERIVED_CLASSES (the compile-time side) in its own TU -
//    an unspecialized primary template silently means "no known bases" to sol2's
//    checker, with no compile error, which is exactly the discrepancy that could
//    not be found before: grunk-occt's own occt_plugin.cpp registers Geom_Curve's
//    bases AND declares its own generated SOL_BASE_CLASSES/SOL_DERIVED_CLASSES in
//    the same translation unit, but a foreign test TU (like the original stand-in,
//    or any plugin consuming grunk-occt's registrations from a different .cpp)
//    does not get that visibility for free.
//
// The test below reproduces cause 2 explicitly by only declaring SOL_BASE_CLASSES/
// SOL_DERIVED_CLASSES for HandleDerived/HandleBase right here, matching what any
// real caller must also do for its own Handle(X) hierarchy.
namespace {

    struct HandleBase { int i; };
    struct HandleDerived : HandleBase {};

    // A minimal opencascade::handle<T>-shaped smart pointer: a bare owning
    // pointer plus an implicit upcast converting constructor from Handle<Derived>
    // to Handle<Base> (mirroring Standard_Handle.hxx's own reference-conversion
    // operator) - deliberately not reference-counted, since ownership semantics
    // are irrelevant to the sol2 binding behavior under test.
    template <typename T>
    struct Handle {
        T* ptr = nullptr;
        Handle() = default;
        explicit Handle(T* p) : ptr(p) {}
        template <typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
        Handle(Handle<U> const& other) : ptr(other.ptr) {}
        bool IsNull() const { return ptr == nullptr; }
        T* get() const { return ptr; }
    };

} // anonymous namespace

namespace sol {
    template <typename T>
    struct unique_usertype_traits<Handle<T>> {
        using type = T;
        using actual_type = Handle<T>;
        static const bool value = true;

        template <typename X>
        using rebind_actual_type = Handle<X>;

        static bool is_null(actual_type const& p) { return p.IsNull(); }
        static type* get(actual_type const& p) { return p.get(); }
    };
} // namespace sol

SOL_BASE_CLASSES(HandleDerived, HandleBase);
SOL_DERIVED_CLASSES(HandleBase, HandleDerived);

TEST(std_vector, with_bases_unique_usertype)
{
    grunk::state grunk;

    grunk.register_type<HandleBase>("HandleBase")
    .add_constructors([](int i){ return Handle<HandleBase>(new HandleBase{i}); })
    .with_std_vector<Handle<HandleBase>>();

    grunk.register_type<HandleDerived>("HandleDerived")
    .add_constructors([](int i){ return Handle<HandleDerived>(new HandleDerived{HandleBase{i}}); })
    .add_bases<HandleBase>();

    auto env = grunk.create_env();
    env.eval(R"(
d1 = HandleDerived.new(5)
d2 = HandleDerived.new(7)
b1 = HandleBase.new(9)

l = HandleBase.as_vec(d1, d2, b1)
)");

    sol::object l = env["l"];
    ASSERT_TRUE(l.is<std::vector<Handle<HandleBase>>>());
    auto lv = l.as<std::vector<Handle<HandleBase>>>();
    ASSERT_EQ(lv.size(), 3u);
    ASSERT_FALSE(lv[0].IsNull());
    ASSERT_FALSE(lv[1].IsNull());
    ASSERT_FALSE(lv[2].IsNull());
    EXPECT_EQ(lv[0].get()->i, 5);
    EXPECT_EQ(lv[1].get()->i, 7);
    EXPECT_EQ(lv[2].get()->i, 9);
}