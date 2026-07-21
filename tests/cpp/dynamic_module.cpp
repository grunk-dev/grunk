// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>
#include <fstream>
#include <cstdio>

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

    // both functions should be available in a plain, non-parametric environment
    auto env = grunk.create_env();
    env.eval(R"(
        a = 2
        b = 3
        c = mymod.if_then_else(mymod.less_than(a,b), 5, 32)
    )");
    auto c = env.get("c");
    ASSERT_EQ(c.as<int>(), 5);

    // both functions should also be available in a parametric environment, decorated as actions
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

    // clearing the module removes it from both environments
    grunk.clear_module("mymod");

    auto penv2 = grunk.create_parametric_env();
    penv2.eval(R"(
        if mymod ~= nil then
            error("module still present")
        end
    )");
}

TEST(module, incremental_population)
{
    grunk::state grunk;

    grunk.run_module_script("mymod", "function add_one(x) return x + 1 end");
    grunk.run_module_script("mymod", "function add_two(x) return x + 2 end");

    auto env = grunk.create_env();
    env.eval(R"(
        a = mymod.add_one(1)
        b = mymod.add_two(1)
    )");
    ASSERT_EQ(env.get("a").as<int>(), 2);
    ASSERT_EQ(env.get("b").as<int>(), 3);
}

TEST(module, run_file)
{
    grunk::state grunk;

    std::string filename = "grunk_test_mymod.lua";
    {
        std::ofstream fout(filename);
        fout << R"(
            function add_one(x) return x + 1 end
            function choose(a,b) if a < b then return a else return b end end
        )";
    }

    grunk.run_module_file("mymod_file", filename);
    std::remove(filename.c_str());

    auto env = grunk.create_env();
    env.eval(R"(
        a = 10
        b = mymod_file.add_one(a)
    )");
    ASSERT_EQ(env.get("b").as<int>(), 11);

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
}

TEST(module, run_file_missing_throws)
{
    grunk::state grunk;
    EXPECT_THROW(grunk.run_module_file("mymod", "does_not_exist.lua"), grunk::io_error);
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
    grunk::state grunk;

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

    EXPECT_NEAR(p.value().as<Pnt>().x, 0.1, 1e-15);
    EXPECT_NEAR(p.value().as<Pnt>().y, 0.2, 1e-15);
    EXPECT_NEAR(p.value().as<Pnt>().z, 0.0, 1e-15);

    u.set_value(5.0);
    EXPECT_NEAR(p.value().as<Pnt>().x, 5.0, 1e-15);
    EXPECT_NEAR(p.value().as<Pnt>().y, 0.2, 1e-15);
    EXPECT_NEAR(p.value().as<Pnt>().z, 0.0, 1e-15);
}
