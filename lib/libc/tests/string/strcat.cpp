#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, strcat)
{
    char s[] = "xx\0xxxxxx";
    EXPECT_EQ(s, strcat(s, abcde));
    EXPECT_EQ('a', s[2]);
    EXPECT_EQ('e', s[6]);
    EXPECT_EQ('\0', s[7]);
    EXPECT_EQ('x', s[8]);
    s[0] = '\0';
    EXPECT_EQ(s, strcat(s, abcdx));
    EXPECT_EQ('x', s[4]);
    EXPECT_EQ('\0', s[5]);
    EXPECT_EQ(s, strcat(s, "\0"));
    EXPECT_EQ('\0', s[5]);
    EXPECT_EQ('e', s[6]);
}
