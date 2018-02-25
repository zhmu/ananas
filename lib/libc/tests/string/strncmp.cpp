#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, strncmp)
{
    char cmpabcde[] = "abcde\0f";
    char cmpabcd_[] = "abcde\xfc";
    char empty[] = "";
    char x[] = "x";
    EXPECT_EQ(0, strncmp(abcde, cmpabcde, 5));
    EXPECT_EQ(0, strncmp(abcde, cmpabcde, 10));
    EXPECT_LT(strncmp(abcde, abcdx, 5), 0);
    EXPECT_GT(strncmp(abcdx, abcde, 5), 0);
    EXPECT_LT(strncmp(empty, abcde, 5), 0);
    EXPECT_GT(strncmp(abcde, empty, 5), 0);
    EXPECT_EQ(0, strncmp(abcde, abcdx, 4));
    EXPECT_EQ(0, strncmp(abcde, x, 0));
    EXPECT_LT(strncmp(abcde, x, 1), 0);
    EXPECT_LT(strncmp(abcde, cmpabcd_, 10), 0);
}
