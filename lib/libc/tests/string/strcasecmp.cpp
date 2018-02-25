#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, strcasecmp)
{
    char cmpabcde[] = "abcde";
    char cmpaBCd_[] = "aBCd\xfc";
    char empty[] = "";
    EXPECT_EQ(0, strcasecmp(abcde, cmpabcde));
    EXPECT_LT(strcasecmp(abcde, abcdx), 0);
    EXPECT_GT(strcasecmp(abcdx, abcde), 0);
    EXPECT_LT(strcasecmp(empty, abcde), 0);
    EXPECT_GT(strcasecmp(abcde, empty), 0);
    EXPECT_LT(strcasecmp(abcde, cmpaBCd_), 0);
}
