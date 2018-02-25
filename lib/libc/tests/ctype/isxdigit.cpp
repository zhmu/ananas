#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, isxdigit)
{
    EXPECT_TRUE(isxdigit('0'));
    EXPECT_TRUE(isxdigit('9'));
    EXPECT_TRUE(isxdigit('a'));
    EXPECT_TRUE(isxdigit('f'));
    EXPECT_FALSE(isxdigit('g'));
    EXPECT_TRUE(isxdigit('A'));
    EXPECT_TRUE(isxdigit('F'));
    EXPECT_FALSE(isxdigit('G'));
    EXPECT_FALSE(isxdigit('@'));
    EXPECT_FALSE(isxdigit(' '));
}
