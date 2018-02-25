#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, strcoll)
{
    char cmpabcde[] = "abcde";
    char empty[] = "";
    EXPECT_EQ(0, strcmp(abcde, cmpabcde));
    EXPECT_LT(strcmp(abcde, abcdx), 0);
    EXPECT_GT(strcmp(abcdx, abcde), 0);
    EXPECT_LT(strcmp(empty, abcde), 0);
    EXPECT_GT(strcmp(abcde, empty), 0);
}
