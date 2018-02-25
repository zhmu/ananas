#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, strcmp)
{
    char cmpabcde[] = "abcde";
    char cmpabcd_[] = "abcd\xfc";
    char empty[] = "";
    EXPECT_EQ(0, strcmp(abcde, cmpabcde));
    EXPECT_LT(strcmp(abcde, abcdx), 0);
    EXPECT_GT(strcmp(abcdx, abcde), 0);
    EXPECT_LT(strcmp(empty, abcde), 0);
    EXPECT_GT(strcmp(abcde, empty), 0);
    EXPECT_LT(strcmp(abcde, cmpabcd_), 0);
}
