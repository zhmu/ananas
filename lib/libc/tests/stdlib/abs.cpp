#include <gtest/gtest.h>
#include <stdlib.h>
#include <limits.h>

TEST(stdlib, abs)
{
    EXPECT_EQ(0, abs(0));
    EXPECT_EQ(INT_MAX, abs(INT_MAX));
    EXPECT_EQ(-(INT_MIN + 1), abs(INT_MIN + 1));
}
