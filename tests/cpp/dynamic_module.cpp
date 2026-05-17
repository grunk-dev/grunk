// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>

TEST(module, basic)
{
    grunk::state grunk;

    grunk.run_module_script(
        "mymod", 
        R"(
            function less_than(l,r)
                return l<r
            end

            function if_then_else(cond, i, e)
                if cond then 
                    return i
                else 
                    return e
                end
            end
        )"
    );

    // both functions should be available in normal env
    auto env = grunk.create_env();
    env.eval(R"(
        a = 2
        b = 3
        c = mymod.if_then_else(mymod.less_than(a,b), 5, 32)
    )");
    auto c = env.get("c");
    ASSERT_EQ(c.as<int>(), 5);

    // both functions should be available in parametric env
    auto penv = grunk.create_parametric_env();
    penv.eval(R"(
        a = grunk.feature(2)
        b = grunk.feature(3)
        c = mymod.if_then_else(mymod.less_than(a,b), 5, 32)
    )");

    auto pc = penv.get_feature("c");
    ASSERT_EQ(pc.value().as<int>(), 5);

    auto pa = penv.get_feature("a");
    pa.set_value(4);
    ASSERT_EQ(pc.value().as<int>(), 32);

    // now clear the module and ensure it's removed from the original environment
    grunk.clear_module("mymod");

    // new parametric env should no longer find mymod
    auto penv2 = grunk.create_parametric_env();
    // accessing mymod should yield nil; evaluate a small snippet and ensure it errors or returns nil
    penv2.eval(R"(
        if mymod ~= nil then
            error("module still present")
        end
    )");
}

TEST(module, run_file)
{
    grunk::state grunk;

    // write a small module to a temp file
    std::string filename = "/tmp/grunk_test_mymod.lua";
    std::ofstream fout(filename);
    fout << R"(
        function add_one(x) return x + 1 end
        function choose(a,b) if a < b then return a else return b end end
    )";
    fout.close();

    // load the module from file
    grunk.run_module_file("mymod_file", filename);

    // normal env should work
    auto env = grunk.create_env();
    env.eval(R"(
        a = 10
        b = mymod_file.add_one(a)
    )");
    ASSERT_EQ(env.get("b").as<int>(), 11);

    // parametric env should also work
    auto penv = grunk.create_parametric_env();
    penv.eval(R"(
        a = grunk.feature(5)
        b = mymod_file.add_one(a)
    )");
    auto fb = penv.get_feature("b");
    ASSERT_EQ(fb.value().as<int>(), 6);

    auto fa = penv.get_feature("a");
    fa.set_value(41);
    ASSERT_EQ(fb.value().as<int>(), 42);

    // cleanup
    std::remove(filename.c_str());
}

namespace {

struct Pnt
{
    Pnt() = default;

    inline void set_x(double x_) { x = x_; }
    inline void set_y(double y_) { y = y_; }
    inline void set_z(double z_) { z = z_; }
    double x{0.};
    double y{0.};
    double z{0.};
};

} // anonymous namespace

TEST(module, nonconst_setters)
{
    auto grunk = grunk::state();

    grunk.register_type<Pnt>("Pnt")
        .add_constructors<>([](){ return Pnt(); })
        .add_member_function("set_x", &Pnt::set_x)
        .add_member_function("set_y", &Pnt::set_y)
        .add_member_function("set_z", &Pnt::set_z);

    grunk.run_module_script(
        "mymod",
        R"(
            function create_pnt(u,v)
                p = Pnt.new()
                p:set_x(u)
                p:set_y(v)
                return p
            end
        )"
    );

    auto u = grunk.feature(0.1);
    auto v = grunk.feature(0.2);
    auto p = grunk.action("mymod.create_pnt", u, v);

    ASSERT_EQ(p.value().as<Pnt>().x, 0.1);
    ASSERT_EQ(p.value().as<Pnt>().y, 0.2);
    ASSERT_EQ(p.value().as<Pnt>().z, 0.0);

    u.set_value(5.0);
    ASSERT_EQ(p.value().as<Pnt>().x, 5.0);
    ASSERT_EQ(p.value().as<Pnt>().y, 0.2);
    ASSERT_EQ(p.value().as<Pnt>().z, 0.0);
}

TEST(module_action, invalidation)
{
    grunk::state gr;
    gr.set_module_source("mymod", "function f(x) return x+1 end");
    auto a = gr.feature(2);
    auto out = gr.action("mymod.f", a);
    ASSERT_EQ(out.value().as<int>(), 3);
    gr.set_module_source("mymod", "function f(x) return x*2 end");
    ASSERT_EQ(out.value().as<int>(), 4);
}

TEST(module_action, action_auto_detect)
{
    grunk::state gr;
    gr.set_module_source("calc", "function add(a, b) return a + b end");
    gr.register_function("normal_func", [](int x) { return x * 2; });
    
    auto x = gr.feature(5);
    auto y = gr.feature(3);
    
    auto mod_result = gr.action("calc.add", x, y);
    ASSERT_EQ(mod_result.value().as<int>(), 8);
    
    auto reg_result = gr.action("normal_func", x);
    ASSERT_EQ(reg_result.value().as<int>(), 10);
    
    x.set_value(10);
    ASSERT_EQ(mod_result.value().as<int>(), 13);
    ASSERT_EQ(reg_result.value().as<int>(), 20);
}