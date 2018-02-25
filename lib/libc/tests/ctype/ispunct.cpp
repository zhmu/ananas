#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, ispunct)
{
    EXPECT_FALSE(ispunct('a'));
    EXPECT_FALSE(ispunct('z'));
    EXPECT_FALSE(ispunct('A'));
    EXPECT_FALSE(ispunct('Z'));
    EXPECT_TRUE(ispunct('@'));
    EXPECT_TRUE(ispunct('.'));
    EXPECT_FALSE(ispunct('\t'));
    EXPECT_FALSE(ispunct('\0'));
    EXPECT_FALSE(ispunct(' '));
}
