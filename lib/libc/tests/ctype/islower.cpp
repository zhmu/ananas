#include <gtest/gtest.h>
#include <ctype.h>

TEST(ctype, islower)
{
    EXPECT_TRUE(islower('a'));
    EXPECT_TRUE(islower('z'));
    EXPECT_FALSE(islower('A'));
    EXPECT_FALSE(islower('Z'));
    EXPECT_FALSE(islower(' '));
    EXPECT_FALSE(islower('@'));
}
