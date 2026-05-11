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

/*
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
*/
}