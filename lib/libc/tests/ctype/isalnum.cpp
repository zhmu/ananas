#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, isalnum)
{
    EXPECT_TRUE(isalnum('a'));
    EXPECT_TRUE(isalnum('z'));
    EXPECT_TRUE(isalnum('A'));
    EXPECT_TRUE(isalnum('Z'));
    EXPECT_TRUE(isalnum('0'));
    EXPECT_TRUE(isalnum('9'));
    EXPECT_FALSE(isalnum(' '));
    EXPECT_FALSE(isalnum('\n'));
    EXPECT_FALSE(isalnum('@'));
}
