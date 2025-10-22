#include <gtest/gtest.h>
#include <grunk/dynamic/state.hpp>

namespace {

    double add(double l, double r) {
        return l + r;
    }

} // anonymous namespace

TEST(function_registry, free_function_lookup_by_name)
{
    grunk::state grunk;
    grunk.register_function("add", &add, {{"l"}, {"r"}});
    sol::protected_function f = grunk.get_function("add"); // just to see that it exists
    ASSERT_TRUE(f.valid());

    auto registry = grunk.get_registry();

    sol::object entry_obj = registry["add"];
    ASSERT_TRUE(entry_obj.is<grunk::function_metadata>());

    auto entry = entry_obj.as<grunk::function_metadata>();
    EXPECT_EQ(entry.name, "add");
    ASSERT_EQ(entry.params->size(), 2);
    EXPECT_EQ(entry.params.value()[0].name.value(), "l");
    EXPECT_EQ(entry.params.value()[1].name.value(), "r");
}

TEST(function_registry, free_function_lookup_by_function)
{
    grunk::state grunk;
    grunk.register_function("add", &add, {{"l"}, {"r"}});
    sol::protected_function f = grunk.get_function("add"); // just to see that it exists
    ASSERT_TRUE(f.valid());

    auto entry = grunk::get_metadata(f);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->name, "add");
    ASSERT_EQ(entry->params->size(), 2);
    EXPECT_EQ(entry->params.value()[0].name.value(), "l");
    EXPECT_EQ(entry->params.value()[1].name.value(), "r");
}

TEST(function_registry, operators_lookup_by_name)
{
    grunk::state grunk;
    auto registry = grunk.get_registry();
    std::vector<std::string> operator_names = {
        "grunk._dynamic_add",
        "grunk._dynamic_sub",
        "grunk._dynamic_mul",
        "grunk._dynamic_div",
        "grunk._dynamic_mod",
        "grunk._dynamic_pow",
        "grunk._dynamic_unm"
    };
    for (auto const& name : operator_names) {
        sol::object entry_obj = registry[name];
        ASSERT_TRUE(entry_obj.is<grunk::function_metadata>());

        auto entry = entry_obj.as<grunk::function_metadata>();
        EXPECT_EQ(entry.name, name);
        if (name == "grunk._dynamic_unm") {
            EXPECT_EQ(entry.params->size(), 1);
            EXPECT_EQ(entry.params.value()[0].name.value(), "value");
        } else if (name == "grunk._dynamic_pow") {
            EXPECT_EQ(entry.params->size(), 2);
            EXPECT_EQ(entry.params.value()[0].name.value(), "base");
            EXPECT_EQ(entry.params.value()[1].name.value(), "exponent");
        } else {
            EXPECT_EQ(entry.params->size(), 2);
            EXPECT_EQ(entry.params.value()[0].name.value(), "lhs");
            EXPECT_EQ(entry.params.value()[1].name.value(), "rhs");
        }
    }
}

TEST(function_registry, operators_lookup_by_function)
{
    grunk::state grunk;
    auto registry = grunk.get_registry();
    std::vector<std::string> operator_names = {
        "_dynamic_add",
        "_dynamic_sub",
        "_dynamic_mul",
        "_dynamic_div",
        "_dynamic_mod",
        "_dynamic_pow",
        "_dynamic_unm"
    };
    for (auto const& name : operator_names) {
        sol::table g = grunk["grunk"];
        sol::protected_function f = g[name];
        ASSERT_TRUE(f.valid());

        auto entry = grunk::get_metadata(f);
        ASSERT_TRUE(entry);
        EXPECT_EQ(entry->name, "grunk." + name);
        if (name == "_dynamic_unm") {
            EXPECT_EQ(entry->params->size(), 1);
            EXPECT_EQ(entry->params.value()[0].name.value(), "value");
        } else if (name == "_dynamic_pow") {
            EXPECT_EQ(entry->params->size(), 2);
            EXPECT_EQ(entry->params.value()[0].name.value(), "base");
            EXPECT_EQ(entry->params.value()[1].name.value(), "exponent");
        } else {
            EXPECT_EQ(entry->params->size(), 2);
            ASSERT_TRUE(entry->params);
            EXPECT_EQ(entry->params.value()[0].name.value(), "lhs");
            EXPECT_EQ(entry->params.value()[1].name.value(), "rhs");
        }
    }
}

namespace {

class MyScalar
{
public:
    MyScalar(double v) : m_value(v) {}
    double value() const
    {
        return m_value;
    }
    void set(double v) {
        m_value = v;
    }
private:
    double m_value;
};

} // anonymous namespace

TEST(function_registry, member_function_lookup_by_name)
{
    grunk::state grunk;
    grunk.register_type<MyScalar>("MyScalar")
    .add_constructors<MyScalar(double)>()
    .add_member_function("value", &MyScalar::value)
    .add_member_function("set", &MyScalar::set, {{"v"}});

    auto registry = grunk.get_registry();

    sol::object entry_obj = registry["MyScalar.value"];
    ASSERT_TRUE(entry_obj.is<grunk::function_metadata>());

    auto entry = entry_obj.as<grunk::function_metadata>();
    EXPECT_EQ(entry.name, "MyScalar.value");
    ASSERT_TRUE(entry.params);
    ASSERT_EQ(entry.params->size(), 0);

    entry_obj = registry["MyScalar.set"];
    ASSERT_TRUE(entry_obj.is<grunk::function_metadata>());
    entry = entry_obj.as<grunk::function_metadata>();
    EXPECT_EQ(entry.name, "MyScalar.set");
    ASSERT_TRUE(entry.params);
    ASSERT_EQ(entry.params->size(), 1);
    ASSERT_TRUE(entry.params.value()[0].name);
    EXPECT_EQ(entry.params.value()[0].name.value(), "v");
}