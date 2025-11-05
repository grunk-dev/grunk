#include <gtest/gtest.h>
#include "grunk/dynamic/ParallelExecutor.hpp"
#include "grunk/dynamic/action.hpp"

TEST(multithreading, simple)
{
    auto x1 = grunk::feature(1.);
    auto x2 = grunk::action([](double v){ return v + 1.; }, x1).output();
    x2.set_id("x2");

    auto y1 = grunk::feature(2.);
    auto y2 = grunk::action([](double v){ return v * 2.; }, y1).output();
    y2.set_id("y2");

    auto z = grunk::action([](double a, double b){ return a + b; }, x2, y2).output();
    z.set_id("z");

    grunk::ParallelExecutor executor(z);
    executor.run();
    EXPECT_TRUE(z.is_valid());
    EXPECT_TRUE(x2.is_valid());
    EXPECT_TRUE(y2.is_valid());
    EXPECT_NEAR(z.value(), 6., 1e-14);
}