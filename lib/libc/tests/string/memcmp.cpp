#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, memcmp)
{
    char const xxxxx[] = "xxxxx";
    EXPECT_LT(memcmp(abcde, abcdx, 5), 0);
    EXPECT_EQ(0, memcmp(abcde, abcdx, 4));
    EXPECT_EQ(0, memcmp(abcde, xxxxx, 0));
    EXPECT_GT(memcmp(xxxxx, abcde, 1), 0);
}
