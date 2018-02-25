#include <gtest/gtest.h>
#include <string.h>

static char const abcde[] = "abcde";

TEST(string, strrchr)
{
    char abccd[] = "abccd";
    EXPECT_EQ(&abcde[5],  strrchr(abcde, '\0'));
    EXPECT_EQ(&abcde[4],  strrchr(abcde, 'e'));
    EXPECT_EQ(&abcde[0], strrchr(abcde, 'a'));
    EXPECT_EQ(&abccd[3], strrchr(abccd, 'c'));
}
