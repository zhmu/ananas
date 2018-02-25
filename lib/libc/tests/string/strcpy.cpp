#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";

TEST(string, strcpy)
{
    char s[] = "xxxxx";
    EXPECT_EQ(s, strcpy(s, ""));
    EXPECT_EQ('\0', s[0]);
    EXPECT_EQ('x', s[1]);
    EXPECT_EQ(s, strcpy(s, abcde));
    EXPECT_EQ('a', s[0]);
    EXPECT_EQ('e', s[4]);
    EXPECT_EQ('\0', s[5]);
}
