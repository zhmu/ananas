#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";

TEST(string, memcpy)
{
    char s[] = "xxxxxxxxxxx";
    EXPECT_EQ(s, memcpy(s, abcde, 6));
    EXPECT_EQ('e', s[4]);
    EXPECT_EQ('\0', s[5]);
    EXPECT_EQ(s + 5, memcpy(s + 5, abcde, 5));
    EXPECT_EQ('e', s[9]);
    EXPECT_EQ('x', s[10]);
}
