#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, isblank)
{
    EXPECT_TRUE(isblank(' '));
    EXPECT_TRUE(isblank('\t'));
    EXPECT_FALSE(isblank('\v'));
    EXPECT_FALSE(isblank('\r'));
    EXPECT_FALSE(isblank('x'));
    EXPECT_FALSE(isblank('@'));
}
