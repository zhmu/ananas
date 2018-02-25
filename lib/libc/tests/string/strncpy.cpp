#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, strncpy)
{
    char s[] = "xxxxxxx";
    EXPECT_EQ(s, strncpy(s, "", 1));
    EXPECT_EQ('\0', s[0]);
    EXPECT_EQ('x', s[1]);
    EXPECT_EQ(s, strncpy(s, abcde, 6));
    EXPECT_EQ('a', s[0]);
    EXPECT_EQ('e', s[4]);
    EXPECT_EQ('\0', s[5]);
    EXPECT_EQ('x', s[6]);
    EXPECT_EQ(s, strncpy(s, abcde, 7));
    EXPECT_EQ('\0', s[6]);
    EXPECT_EQ(s, strncpy(s, "xxxx", 3));
    EXPECT_EQ('x', s[0]);
    EXPECT_EQ('x', s[2]);
    EXPECT_EQ('d', s[3]);
}
