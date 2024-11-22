#include <gtest/gtest.h>
#include <state.hpp>

namespace {

void fun(){};

struct Foo
{
    int baz(std::string) const { return 42; }
    double bar(bool){ return 3.145; }
};

} // anonymous namespace

TEST(metadata, free_function)
{
    grunk::state grunk;
    grunk.register_function("fun", fun);
    auto f = grunk.get_function("fun");

    sol::state_view lua(f.lua_state());
    EXPECT_EQ(grunk::get_metadata(lua, f, "name").as<std::string>(), "fun");
    EXPECT_TRUE(grunk::get_metadata(lua, f, "is_pure").as<bool>());
}

TEST(metadata, member_function)
{
    grunk::state grunk;
    grunk.register_type<Foo>("Foo")
    .add_member_function("baz", &Foo::baz)
    .add_member_function("bar", &Foo::bar);
    auto f = grunk.get_function("fun");

    sol::state_view lua(f.lua_state());
    EXPECT_EQ(grunk::get_metadata(lua, grunk.get_function("Foo.baz"), "name").as<std::string>(), "baz");
    EXPECT_TRUE(grunk::get_metadata(lua, grunk.get_function("Foo.baz"), "is_pure").as<bool>());

    EXPECT_EQ(grunk::get_metadata(lua, grunk.get_function("Foo.bar"), "name").as<std::string>(), "bar");
    EXPECT_FALSE(grunk::get_metadata(lua, grunk.get_function("Foo.bar"), "is_pure").as<bool>());
}
