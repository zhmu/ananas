#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";

TEST(string, strlen)
{
    EXPECT_EQ(5, strlen(abcde));
    EXPECT_EQ(0, strlen(""));
}
