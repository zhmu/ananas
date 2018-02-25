#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, isdigit)
{
    EXPECT_TRUE(isdigit('0'));
    EXPECT_TRUE(isdigit('9'));
    EXPECT_FALSE(isdigit(' '));
    EXPECT_FALSE(isdigit('a'));
    EXPECT_FALSE(isdigit('@'));
}
