#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, isalpha)
{
    EXPECT_TRUE(isalpha('a'));
    EXPECT_TRUE(isalpha('z'));
    EXPECT_FALSE(isalpha(' '));
    EXPECT_FALSE(isalpha('1'));
    EXPECT_FALSE(isalpha('@'));
}
