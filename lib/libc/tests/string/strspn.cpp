#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";

TEST(string, strspn)
{
    EXPECT_EQ(3, strspn(abcde, "abc"));
    EXPECT_EQ(0, strspn(abcde, "b"));
    EXPECT_EQ(5, strspn(abcde, abcde));
}
