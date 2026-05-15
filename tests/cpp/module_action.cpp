// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include <grunk/dynamic.hpp>

TEST(module_action, invalidation)
{
    grunk::state gr;
    // initial script: add 1
    gr.set_module_source("mymod", "function f(x) return x+1 end");
    auto a = gr.feature(2);
    auto out = gr.action_from_module("mymod", "f", a);
    EXPECT_EQ(out.value().as<int>(), 3);
    // change script to multiply by 2
    gr.set_module_source("mymod", "function f(x) return x*2 end");
    // accessing out again should recompute with new definition
    EXPECT_EQ(out.value().as<int>(), 4);
}
