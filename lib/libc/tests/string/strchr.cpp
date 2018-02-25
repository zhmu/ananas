#include <gtest/gtest.h>
#include <string.h>

TEST(string, strchr)
{
    char abccd[] = "abccd";
    EXPECT_EQ(NULL, strchr(abccd, 'x'));
    EXPECT_EQ(&abccd[0], strchr(abccd, 'a'));
    EXPECT_EQ(&abccd[4], strchr(abccd, 'd'));
    EXPECT_EQ(&abccd[5], strchr(abccd, '\0'));
    EXPECT_EQ(&abccd[2], strchr(abccd, 'c'));
}
