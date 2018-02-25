#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, toupper)
{
    EXPECT_EQ('A', toupper('a'));
    EXPECT_EQ('Z', toupper('z'));
    EXPECT_EQ('A', toupper('A'));
    EXPECT_EQ('Z', toupper('Z'));
    EXPECT_EQ('@', toupper('@'));
    EXPECT_EQ('[', toupper('['));
}
