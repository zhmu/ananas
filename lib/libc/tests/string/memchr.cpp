#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";
static char const abcdx[] = "abcdx";

TEST(string, memchr)
{
    EXPECT_EQ(&abcde[2], memchr(abcde, 'c', 5));
    EXPECT_EQ(&abcde[0], memchr(abcde, 'a', 1));
    EXPECT_EQ(NULL, memchr(abcde, 'a', 0));
    EXPECT_EQ(NULL, memchr(abcde, '\0', 5));
    EXPECT_EQ(&abcde[5], memchr(abcde, '\0', 6));
}
