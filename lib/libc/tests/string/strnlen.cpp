#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";
TEST(string, strnlen)
{
    EXPECT_EQ(5, strnlen(abcde, 5));
    EXPECT_EQ(3, strnlen(abcde, 3));
    EXPECT_EQ(0, strnlen("", SIZE_MAX));
}
