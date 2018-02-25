#include <gtest/gtest.h>
#include <string.h>

TEST(string, memmove)
{
    char s[] = "xxxxabcde";
    EXPECT_EQ(s, memmove(s, s + 4, 5));
    EXPECT_EQ('a', s[0]);
    EXPECT_EQ('e', s[4]);
    EXPECT_EQ('b', s[5]);
    EXPECT_EQ(s + 4, memmove(s + 4, s, 5));
    EXPECT_EQ('a', s[4]);
}
