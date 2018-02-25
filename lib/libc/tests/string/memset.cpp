#include <gtest/gtest.h>
#include <string.h>

TEST(string, memset)
{
    char s[] = "xxxxxxxxx";
    EXPECT_EQ(s, memset(s, 'o', 10));
    EXPECT_EQ('o', s[9]);
    EXPECT_EQ(s, memset(s, '_', 0));
    EXPECT_EQ('o', s[0]);
    EXPECT_EQ(s, memset(s, '_', 1));
    EXPECT_EQ('_', s[0]);
    EXPECT_EQ('o', s[1]);
}
