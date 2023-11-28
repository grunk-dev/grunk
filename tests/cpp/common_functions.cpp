#include <gtest/gtest.h>

#include <grunk/common/common_functions.hpp>

TEST(common_functions, split_string)
{
    {
    auto r = grunk::split("ab:cd", ":");
    EXPECT_EQ(r.size(), 2);
    EXPECT_EQ(r[0], "ab");
    EXPECT_EQ(r[1], "cd");
    }
    {
    auto r = grunk::split("abcd", ":");
    EXPECT_EQ(r.size(), 1);
    EXPECT_EQ(r[0], "abcd");
    }
    {
    auto r = grunk::split("abcd", "d");
    EXPECT_EQ(r.size(), 2);
    EXPECT_EQ(r[0], "abc");
    EXPECT_EQ(r[1], "");
    }

    {
    auto r = grunk::split("abcd", "a");
    EXPECT_EQ(r.size(), 2);
    EXPECT_EQ(r[0], "");
    EXPECT_EQ(r[1], "bcd");
    }

    {
    auto r = grunk::split("ab::cd", ":");
    EXPECT_EQ(r.size(), 3);
    EXPECT_EQ(r[0], "ab");
    EXPECT_EQ(r[1], "");
    EXPECT_EQ(r[2], "cd");
    }

    {
    auto r = grunk::split("ab::cd", "::c");
    EXPECT_EQ(r.size(), 2);
    EXPECT_EQ(r[0], "ab");
    EXPECT_EQ(r[1], "d");
    }
}