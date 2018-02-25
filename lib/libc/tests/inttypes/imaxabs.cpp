#include <gtest/gtest.h>
#include <inttypes.h>

TEST(inttypes, imaxabs)
{
    EXPECT_EQ(0, imaxabs((intmax_t)0));
    EXPECT_EQ(INTMAX_MAX, imaxabs(INTMAX_MAX));
    EXPECT_EQ(-(INTMAX_MIN + 1), imaxabs(INTMAX_MIN + 1));
}
