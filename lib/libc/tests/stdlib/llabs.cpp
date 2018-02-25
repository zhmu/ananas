#include <gtest/gtest.h>
#include <stdlib.h>
#include <limits.h>

TEST(stdlib, llabs)
{
    EXPECT_EQ(0, llabs(0));
    EXPECT_EQ(LLONG_MAX, llabs(LLONG_MAX));
    EXPECT_EQ(-(LLONG_MIN + 1), llabs(LLONG_MIN + 1));
}
