#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, tolower)
{
    EXPECT_EQ('a', tolower('A'));
    EXPECT_EQ('z', tolower('Z'));
    EXPECT_EQ('a', tolower('a'));
    EXPECT_EQ('z', tolower('z'));
    EXPECT_EQ('@', tolower('@'));
    EXPECT_EQ('[', tolower('['));
}
