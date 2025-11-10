#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>

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

TEST(serialization, DISABLED_nonserializable){
    // TODO: This doesn't throw but crashes deep in a sol call. 
    // Might be a sol bug, I am not sure. Will try to figure it out later
    grunk::state grunk;
    auto x = grunk.feature(Foo(99, "red balloons"));
    EXPECT_THROW(x.node_pointer()->serialize(), grunk::io_error);
}

TEST(serialization, no_serialize_method){
    // TODO: This doesn't throw but crashes deep in a sol call. 
    // Might be a sol bug, I am not sure. Will try to figure it out later
    grunk::state grunk;

    grunk.register_type<Foo>("Foo")
    .add_constructors(
        [](int i, std::string_view s) { return Foo(i, s); }
    );

    auto x = grunk.feature(Foo(99, "red balloons"));
    EXPECT_THROW(x.node_pointer()->serialize(), grunk::io_error);
}

TEST(serialization, userdata)
{
    grunk::state grunk;

    grunk.register_type<Foo>("Foo")
    .add_constructors(
        [](int i, std::string_view s) { return Foo(i, s); }
    )
    .set(sol::meta_function::to_string,
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

    auto ret = z.compute_node()->serialize();
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

    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = add(2.0, y)");
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

    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = add(x, 3.0)");
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

    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = add(2.0, 3.0)");
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

    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "add(2.0, 3.0)");
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

    auto ret = w.compute_node()->serialize();
    EXPECT_EQ(ret, "w = add(add(x, 3.0), 5)");
}

TEST(serialization, operator_action_lua_add)
{
    grunk::state grunk;

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x + y 
    )");
    auto z = env.get_feature("z");

    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x + y");
}

TEST(serialization, operator_action_cpp_add)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x + y;
    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x + y");
}

TEST(serialization, operator_action_cpp_add_anonymous1)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x + y;
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "(x + y)");
}

TEST(serialization, operator_action_cpp_add_anonymous2)
{
    grunk::state grunk;
    auto x = grunk.feature(2.);
    auto y = grunk.feature(3.).with_id("y");
    auto z = x + y;
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "(2.0 + y)");
}

TEST(serialization, operator_action_cpp_add_anonymous_nested)
{
    grunk::state grunk;
    auto x = grunk.feature(2.);
    auto y = grunk.feature(3.).with_id("y");
    auto z = x + y;
    auto w = z + 5;
    w.set_id("w");
    auto ret = w.compute_node()->serialize();
    EXPECT_EQ(ret, "w = (2.0 + y) + 5");
}

TEST(serialization, operator_action_lua_sub)
{
    grunk::state grunk;

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x - y
    )");
    auto z = env.get_feature("z");

    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x - y");
}

TEST(serialization, operator_action_cpp_sub)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x - y;
    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x - y");
}

TEST(serialization, operator_action_lua_mul)
{
    grunk::state grunk;

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x * y
    )");
    auto z = env.get_feature("z");

    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x * y");
}

TEST(serialization, operator_action_cpp_mul)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x * y;
    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x * y");
}

TEST(serialization, operator_action_lua_div)
{
    grunk::state grunk;

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x / y
    )");
    auto z = env.get_feature("z");

    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x / y");
}

TEST(serialization, operator_action_cpp_div)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = x / y;
    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x / y");
}

TEST(serialization, operator_action_lua_pow)
{
    grunk::state grunk;

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x ^ y
    )");
    auto z = env.get_feature("z");

    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x ^ y");
}

TEST(serialization, operator_action_cpp_pow)
{
    grunk::state grunk;
    auto x = grunk.feature(2.).with_id("x");
    auto y = grunk.feature(3.).with_id("y");
    auto z = grunk::pow(x,y);
    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x ^ y");
}

TEST(serialization, operator_action_lua_mod)
{
    grunk::state grunk;

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = grunk.feature(2.):with_id("x")
        y = grunk.feature(3.):with_id("y")
        z = x % y 
    )");
    auto z = env.get_feature("z");

    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x % y");
}

TEST(serialization, operator_action_cpp_mod)
{
    grunk::state grunk;
    auto x = grunk.feature(2).with_id("x");
    auto y = grunk.feature(3).with_id("y");
    auto z = x % y;
    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = x % y");
}

TEST(serialization, operator_action_lua_unm)
{
    grunk::state grunk;

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = grunk.feature(2.):with_id("x")
        z = -x
    )");
    auto z = env.get_feature("z");

    z.set_id("z");
    EXPECT_EQ(z.compute_node()->get_parents().size(), 1);
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = -x");
}

TEST(serialization, operator_action_cpp_unm)
{
    grunk::state grunk;
    auto x = grunk.feature(2).with_id("x");
    auto z = -x;
    z.set_id("z");
    auto ret = z.compute_node()->serialize();
    EXPECT_EQ(ret, "z = -x");
}

namespace {

    struct Dummy {

        Dummy() = default;
        Dummy(double v) : m_value{v} {}

        void set_value(double v) {
            m_value = v;
        }
        double value() const {
            return m_value;
        }

        double m_value{0.0};
    };
} // anonymous namespace

TEST(serialization, member_function_action_cpp)
{
    grunk::state grunk;
    grunk.register_type<Dummy>("Dummy")
    .add_constructors(
        []() { return Dummy(); },
        [](double v) { return Dummy(v); }
    )
    .add_member_function("set_value", &Dummy::set_value)
    .add_member_function("value", &Dummy::value);

    auto a = grunk.feature(Dummy()).with_id("a");

    auto x = grunk.action("Dummy.set_value", a, 4.).with_id("x");
    auto ret_x = x.compute_node()->serialize();
    EXPECT_EQ(ret_x, "x = Dummy.set_value(a, 4.0)");

    auto y = grunk.action("Dummy.value", a).with_id("y");   
    auto ret_y = y.compute_node()->serialize();
    EXPECT_EQ(ret_y, "y = Dummy.value(a)");
}

TEST(serialization, member_function_action_lua)
{
    grunk::state grunk;
    grunk.register_type<Dummy>("Dummy")
    .add_constructors(
        []() { return Dummy(); },
        [](double v) { return Dummy(v); }
    )
    .add_member_function("set_value", &Dummy::set_value)
    .add_member_function("value", &Dummy::value);

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        a = Dummy.new_feature():with_id("a")

        x = Dummy.set_value(a, 4.):with_id("x")
        y = Dummy.value(a):with_id("y")
    )");
    auto x = env.get_feature("x");
    auto ret_x = x.compute_node()->serialize();
    EXPECT_EQ(ret_x, "x = Dummy.set_value(a, 4.0)");

    auto y = env.get_feature("y");
    auto ret_y = y.compute_node()->serialize();
    EXPECT_EQ(ret_y, "y = Dummy.value(a)");
}

TEST(serialization, ctor_action_cpp)
{
    grunk::state grunk;

    grunk.register_type<Dummy>("Dummy")
    .add_constructors(
        []() { return Dummy(); },
        [](double v) { return Dummy(v); }
    )
    .add_member_function("set_value", &Dummy::set_value)
    .add_member_function("value", &Dummy::value);

    auto a = grunk.feature(2.).with_id("a");
    auto x = grunk.action("Dummy.new", a).with_id("x");
    auto ret_x = x.compute_node()->serialize();
    EXPECT_EQ(ret_x, "x = Dummy.new(a)");
}

TEST(serialization, ctor_action_lua)
{
    grunk::state grunk;

    grunk.register_type<Dummy>("Dummy")
    .add_constructors(
        []() { return Dummy(); },
        [](double v) { return Dummy(v); }
    )
    .add_member_function("set_value", &Dummy::set_value)
    .add_member_function("value", &Dummy::value);

    auto env = grunk.create_parametric_env();
    env.eval(R"(
        x = Dummy.new(2.):with_id("x")
    )");
    auto x = env.get_feature("x");
    auto ret_x = x.compute_node()->serialize();
    EXPECT_EQ(ret_x, "x = Dummy.new(2.0)");
}

TEST(serialization, Serializer_cpp)
{
    grunk::state grunk;

    auto a = grunk.feature(2.).with_id("a");
    auto b = grunk.feature(3.).with_id("b");
    auto c = a + b;
    c.set_id("c");
    auto d = grunk::pow(c, 2);
    d.set_id("d");

    grunk::Serializer tree;
    tree.parse(d);
    
    auto steps = tree.get_steps();
    ASSERT_EQ(steps.size(), 2);
    EXPECT_EQ(steps[0], "c = a + b");
    EXPECT_EQ(steps[1], "d = c ^ 2");
    
    auto parameters = tree.get_parameters();
    ASSERT_EQ(parameters.size(), 2);
    EXPECT_EQ(parameters.at("a"), "2.0");
    EXPECT_EQ(parameters.at("b"), "3.0");
    
    auto script = tree.get_string(true);
    EXPECT_EQ("\n" + script, R"(
a = grunk.feature(2.0)
b = grunk.feature(3.0)
c = a + b
d = c ^ 2)");

    auto env = grunk.create_parametric_env();
    ASSERT_NO_THROW(env.eval(script));
    EXPECT_NEAR(env.get_feature("d").value().as<double>(), 25, 1e-10);
}

TEST(serialization, Serializer_lua)
{
    grunk::state grunk;
    auto env = grunk.create_parametric_env();
    env.eval(R"(
        a = grunk.feature(2.):with_id("a")
        b = grunk.feature(3.):with_id("b")
        c = a + b -- this is an anonymous feature, therefore this line will not be included and ...
        d = c ^ 2 -- ... this line will be stringified to d = (a + b) ^ 2
        d:set_id("d")
    )");
    auto d = env.get_feature("d");
    grunk::Serializer tree;
    tree.parse(d);
    auto steps = tree.get_steps();
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0], "d = (a + b) ^ 2");

    auto parameters = tree.get_parameters();
    ASSERT_EQ(parameters.size(), 2);
    EXPECT_EQ(parameters.at("a"), "2.0");
    EXPECT_EQ(parameters.at("b"), "3.0");
    auto script = tree.get_string(true);
    EXPECT_EQ("\n" + script, R"(
a = grunk.feature(2.0)
b = grunk.feature(3.0)
d = (a + b) ^ 2)");    

    auto env2 = grunk.create_parametric_env();
    ASSERT_NO_THROW(env2.eval(script));
    EXPECT_NEAR(env2.get_feature("d").value().as<double>(), 25, 1e-10);
}

TEST(serialize, duplicate_name_in_parameters)
{
    grunk::state grunk;
    auto x = grunk.feature(2).with_id("x");
    auto y = grunk.feature(5).with_id("x");
    auto z = x + y;
    grunk::Serializer tree;
    EXPECT_THROW(tree.parse(z), grunk::io_error);
}

TEST(serialize, duplicate_name_in_steps)
{
    grunk::state grunk;
    auto x = grunk.feature(2).with_id("x");
    auto y = grunk.feature(5).with_id("y");
    auto a = x + y;
    auto b = grunk::pow(a,2);
    a.set_id("a");
    b.set_id("a");
    grunk::Serializer tree;
    EXPECT_THROW(tree.parse(b), grunk::io_error);
}

TEST(serialize, userdata_as_parameters)
{
    grunk::state grunk;
    grunk.register_type<Foo>("Foo")
    .add_constructors(
        [](int i, std::string_view s) { return Foo(i, s); }
    )
    .set(sol::meta_function::to_string,
        [](Foo const& foo) {
            return "Foo.new(" + std::to_string(foo.bar) + ", " + "\"" + foo.baz + "\"" + ")";
        }
    );

    grunk.register_function("bar", [](Foo const&){ return 42; });

    auto x = grunk.feature(Foo(99, "red balloons")).with_id("x");
    auto y = grunk.action("bar", x).with_id("y");
    EXPECT_EQ(y.value().as<int>(), 42);
    grunk::Serializer tree;
    tree.parse(y);

    auto steps = tree.get_steps();
    ASSERT_EQ(steps.size(), 1);
    EXPECT_EQ(steps[0], "y = bar(x)");

    auto parameters = tree.get_parameters();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters.at("x"), "Foo.new(99, \"red balloons\")");

    auto script = tree.get_string(true);
    EXPECT_EQ("\n" + script, R"(
x = Foo.new_feature(99, "red balloons")
y = bar(x))");    

    auto env = grunk.create_parametric_env();
    ASSERT_NO_THROW(env.eval(script));
    EXPECT_EQ(env.get_feature("y").value().as<int>(), 42);
}

TEST(serialize, tag_features)
{
    auto get_script = [](auto feature){
        grunk::Serializer tree;
        tree.parse(feature);
        return tree.get_string(true);
    };

    grunk::state grunk;
    auto env = grunk.create_parametric_env();
    env.eval(R"(
x = grunk.feature(2)
y = grunk.feature(8)
z = x * y)");
    auto z = env.get_feature("z");
    
    auto script1 = get_script(z);
    EXPECT_TRUE(script1.empty()); // all features are anonymous! No script generated

    env.tag_features();

    auto script2 = get_script(z);
    EXPECT_EQ("\n" + script2, R"(
x = grunk.feature(2)
y = grunk.feature(8)
z = x * y)");  
}

TEST(serialize, placeholder_feature_cpp)
{
    grunk::state grunk;
    auto x = grunk.feature().with_id("x");
    auto y = grunk.feature().with_id("y");
    auto z = x + y;

    EXPECT_TRUE(x.is_placeholder());
    EXPECT_TRUE(y.is_placeholder());
    EXPECT_FALSE(z.is_placeholder());

    z.set_id("z");

    grunk::Serializer tree;
    tree.parse(z);
    auto script = tree.get_string(true);
    EXPECT_EQ("\n" + script, R"(
x = grunk.feature()
y = grunk.feature()
z = x + y)");
}