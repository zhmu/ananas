#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, isspace)
{
    EXPECT_TRUE(isspace(' '));
    EXPECT_TRUE(isspace('\f'));
    EXPECT_TRUE(isspace('\n'));
    EXPECT_TRUE(isspace('\r'));
    EXPECT_TRUE(isspace('\t'));
    EXPECT_TRUE(isspace('\v'));
    EXPECT_FALSE(isspace('a'));
}
