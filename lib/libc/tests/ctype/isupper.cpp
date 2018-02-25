#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, isupper)
{
    EXPECT_TRUE(isupper('A'));
    EXPECT_TRUE(isupper('Z'));
    EXPECT_FALSE(isupper('a'));
    EXPECT_FALSE(isupper('z'));
    EXPECT_FALSE(isupper(' '));
    EXPECT_FALSE(isupper('@'));
}
