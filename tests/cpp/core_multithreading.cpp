// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include <gtest/gtest.h>
#include "grunk/core.hpp"

namespace {

    void dump_dot_file(grunk::ParallelExecutor& executor)
    {
        // build task flow and get the graphviz representation
        if (executor.get_taskflow().empty()) {
            executor.build_taskflow();
        }
        std::string graphviz = executor.get_taskflow().dump();

        // filename is test_suite underscore test_name underscore taskflow.dot
        std::string test_suite = ::testing::UnitTest::GetInstance()->current_test_info()->test_suite_name();
        std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string filename = test_suite + "_" + test_name + "_taskflow.dot";

        // write to file
        std::ofstream fout(filename);
        fout << graphviz;

    }

}

TEST(multithreading, cpp_static_mode_simple_single_threaded)
{
    /*
    create the following computation graph:
    
        x1(1) --> x2(+1) --
                           \
                            --> z(+)
                           /
        y1(2) --> y2(*2) --
    */
    auto x1 = grunk::feature(1.);
    auto x2 = grunk::action([](double v){ return v + 1.; }, x1);
    x2.set_id("x2");

    auto y1 = grunk::feature(2.);
    auto y2 = grunk::action([](double v){ return v * 2.; }, y1);
    y2.set_id("y2");

    auto z = grunk::action([](double a, double b){ return a + b; }, x2, y2);
    z.set_id("z");

    // single threaded execution
    grunk::ParallelExecutor executor(1, z);
    dump_dot_file(executor);
    executor.run();
    EXPECT_TRUE(z.is_valid());
    EXPECT_TRUE(x2.is_valid());
    EXPECT_TRUE(y2.is_valid());
    EXPECT_NEAR(z.value(), 6., 1e-14);
}

TEST(multithreading, cpp_static_mode_simple)
{
    /*
    create the following computation graph:
    
        x1(1) --> x2(+1) --
                           \
                            --> z(+)
                           /
        y1(2) --> y2(*2) --
    */
    auto x1 = grunk::feature(1.);
    auto x2 = grunk::action([](double v){ return v + 1.; }, x1);
    x2.set_id("x2");

    auto y1 = grunk::feature(2.);
    auto y2 = grunk::action([](double v){ return v * 2.; }, y1);
    y2.set_id("y2");

    auto z = grunk::action([](double a, double b){ return a + b; }, x2, y2);
    z.set_id("z");

    // multi-threaded execution
    grunk::ParallelExecutor executor(z);
    dump_dot_file(executor);
    executor.run();
    EXPECT_TRUE(z.is_valid());
    EXPECT_TRUE(x2.is_valid());
    EXPECT_TRUE(y2.is_valid());
    EXPECT_NEAR(z.value(), 6., 1e-14);
}

TEST(multithreading, cpp_static_mode_simple_shared_ancestor)
{

    /*
    create the following computation graph:
    
                         --> y1(*2) --
                        /             \
         x -->  y(+1) --                --> z(+)
                        \             /
                         --> y2(*3) --
    */
    auto x  = grunk::feature(1.).with_id("x");
    auto y  = grunk::action([](double v){ return v + 1.; }, x).with_id("y");
    auto y1 = grunk::action([](double v){ return v * 2.; }, y).with_id("y1");
    auto y2 = grunk::action([](double v){ return v * 3.; }, y).with_id("y2");
    auto z  = grunk::action([](double a, double b){ return a + b; }, y1, y2).with_id("z");

    grunk::ParallelExecutor executor(z);
    dump_dot_file(executor);
    executor.run();
    EXPECT_TRUE(z.is_valid());
    EXPECT_TRUE(y1.is_valid());
    EXPECT_TRUE(y2.is_valid());
    EXPECT_TRUE(y.is_valid());
    EXPECT_NEAR(z.value(), 10., 1e-14);
}

TEST(multithreading, cpp_static_mode_simple_shared_ancestor2)
{

    /*
    create the following computation graph:
    
                         --> y1(*2) 
                        /
         x -->  y(+1) --
                        \ 
                         --> y2(*3) --
    */
    auto x  = grunk::feature(1.).with_id("x");
    auto y  = grunk::action([](double v){ return v + 1.; }, x).with_id("y");
    auto y1 = grunk::action([](double v){ return v * 2.; }, y).with_id("y1");
    auto y2 = grunk::action([](double v){ return v * 3.; }, y).with_id("y2");

    grunk::ParallelExecutor executor(y1, y2);
    dump_dot_file(executor);
    executor.run();
    EXPECT_TRUE(y2.is_valid());
    EXPECT_TRUE(y1.is_valid());
    EXPECT_TRUE(y.is_valid());
    EXPECT_NEAR(y1.value(), 4., 1e-14);
    EXPECT_NEAR(y2.value(), 6., 1e-14);
}

TEST(multithreading, cpp_static_mode_complex_graph)
{
    /*
    create the following computation graph:
    
        a1(1) --> a2(+1) --> a3(*2) --
                                     \
        b1(2) --> b2(*2) --> b3(+3) -- --> z(+)
                                     /
        c1(3) --> c2(^2) --> c3(/3) --
    */
    auto a1 = grunk::feature(1.);
    auto a2 = grunk::action([](double v){ return v + 1.; }, a1);
    auto a3 = grunk::action([](double v){ return v * 2.; }, a2);

    auto b1 = grunk::feature(2.);
    auto b2 = grunk::action([](double v){ return v * 2.; }, b1);
    auto b3 = grunk::action([](double v){ return v + 3.; }, b2);

    auto c1 = grunk::feature(3.);
    auto c2 = grunk::action([](double v){ return std::pow(v, 2.); }, c1);
    auto c3 = grunk::action([](double v){ return v / 3.; }, c2);

    auto z = grunk::action([](double a, double b, double c){ return a + b + c; }, a3, b3, c3);

    grunk::ParallelExecutor executor(z);
    dump_dot_file(executor);
    executor.run();
    EXPECT_TRUE(z.is_valid());
    EXPECT_TRUE(a3.is_valid());
    EXPECT_TRUE(b3.is_valid());
    EXPECT_TRUE(c3.is_valid());
    EXPECT_NEAR(z.value(), 14., 1e-14);
}
