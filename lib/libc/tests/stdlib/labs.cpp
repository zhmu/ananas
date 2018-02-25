#include <gtest/gtest.h>
#include <stdlib.h>
#include <limits.h>

TEST(stdlib, labs)
{
    EXPECT_EQ(0, labs(0));
    EXPECT_EQ(LONG_MAX, labs(LONG_MAX));
    EXPECT_EQ(-(LONG_MIN + 1), labs(LONG_MIN + 1));
}
