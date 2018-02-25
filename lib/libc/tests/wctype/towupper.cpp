#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, towupper)
{
    EXPECT_EQ(0, towupper(0));
    EXPECT_EQ(L'A', towupper(L'a'));
    EXPECT_EQ(L'B', towupper(L'B'));
    EXPECT_EQ(L'0', towupper(L'0'));
}
