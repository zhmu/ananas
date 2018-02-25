#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, strncasecmp)
{
    char cmpabcde[] = "abcde\0f";
    char cmpaBCd_[] = "aBCde\xfc";
    char empty[] = "";
    char x[] = "x";
    EXPECT_EQ(0, strncasecmp(abcde, cmpabcde, 5));
    EXPECT_EQ(0, strncasecmp(abcde, cmpabcde, 10));
    EXPECT_LT(strncasecmp(abcde, abcdx, 5), 0);
    EXPECT_GT(strncasecmp(abcdx, abcde, 5), 0);
    EXPECT_LT(strncasecmp(empty, abcde, 5), 0);
    EXPECT_GT(strncasecmp(abcde, empty, 5), 0);
    EXPECT_EQ(0, strncasecmp(abcde, abcdx, 4));
    EXPECT_EQ(0, strncasecmp(abcde, x, 0));
    EXPECT_LT(strncasecmp(abcde, x, 1), 0);
    EXPECT_LT(strncasecmp(abcde, cmpaBCd_, 10), 0);
}
