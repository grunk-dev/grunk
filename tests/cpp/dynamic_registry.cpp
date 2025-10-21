#include <gtest/gtest.h>
#include <grunk/dynamic/state.hpp>

namespace {

    double add(double l, double r) {
        return l + r;
    }

} // anonymous namespace

TEST(function_registry, free_function_lookup)
{
    grunk::state grunk;
    grunk.register_function("add", &add, {{"l"}, {"r"}});
    sol::protected_function f = grunk.get_function("add"); // just to see that it exists
    ASSERT_TRUE(f.valid());

    auto registry = grunk.get_registry();

    // lookup by name
    {
        sol::object entry_obj = registry["add"];
        ASSERT_TRUE(entry_obj.is<grunk::function_metadata>());

        auto entry = entry_obj.as<grunk::function_metadata>();
        EXPECT_EQ(entry.name, "add");
        ASSERT_EQ(entry.params.size(), 2);
        EXPECT_EQ(entry.params[0].name.value(), "l");
        EXPECT_EQ(entry.params[1].name.value(), "r");
    }

    // lookup by function
    {
        sol::object entry_obj = registry[f];
        ASSERT_TRUE(entry_obj.is<grunk::function_metadata>());

        auto entry = entry_obj.as<grunk::function_metadata>();
        EXPECT_EQ(entry.name, "add");
        ASSERT_EQ(entry.params.size(), 2);
        EXPECT_EQ(entry.params[0].name.value(), "l");
        EXPECT_EQ(entry.params[1].name.value(), "r");
    }
}