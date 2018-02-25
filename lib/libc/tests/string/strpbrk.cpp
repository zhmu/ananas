#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, strpbrk)
{
    EXPECT_EQ(NULL, strpbrk(abcde, "x"));
    EXPECT_EQ(NULL, strpbrk(abcde, "xyz"));
    EXPECT_EQ(&abcdx[4], strpbrk(abcdx, "x"));
    EXPECT_EQ(&abcdx[4], strpbrk(abcdx, "xyz"));
    EXPECT_EQ(&abcdx[4], strpbrk(abcdx, "zyx"));
    EXPECT_EQ(&abcde[0], strpbrk(abcde, "a"));
    EXPECT_EQ(&abcde[0], strpbrk(abcde, "abc"));
    EXPECT_EQ(&abcde[0], strpbrk(abcde, "cba"));
}
