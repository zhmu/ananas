#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, towlower)
{
    EXPECT_EQ(0, towlower(0));
    EXPECT_EQ(L'a', towlower(L'a'));
    EXPECT_EQ(L'b', towlower(L'B'));
    EXPECT_EQ(L'0', towlower(L'0'));
}
