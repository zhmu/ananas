#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, strcspn)
{
    EXPECT_EQ(5, strcspn(abcde, "x"));
    EXPECT_EQ(5, strcspn(abcde, "xyz"));
    EXPECT_EQ(5, strcspn(abcde, "zyx"));
    EXPECT_EQ(4, strcspn(abcdx, "x"));
    EXPECT_EQ(4, strcspn(abcdx, "xyz"));
    EXPECT_EQ(4, strcspn(abcdx, "zyx"));
    EXPECT_EQ(0, strcspn(abcde, "a"));
    EXPECT_EQ(0, strcspn(abcde, "abc"));
    EXPECT_EQ(0, strcspn(abcde, "cba"));
}
