#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, isprint)
{
    EXPECT_TRUE(isprint('z'));
    EXPECT_TRUE(isprint('z'));
    EXPECT_TRUE(isprint('A'));
    EXPECT_TRUE(isprint('Z'));
    EXPECT_TRUE(isprint('@'));
    EXPECT_FALSE(isprint('\t'));
    EXPECT_FALSE(isprint('\0'));
    EXPECT_TRUE(isprint(' '));
}
