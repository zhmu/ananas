#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswupper)
{
    EXPECT_FALSE(iswupper(L'a'));
    EXPECT_FALSE(iswupper(L'e'));
    EXPECT_FALSE(iswupper(L'z'));
    EXPECT_TRUE(iswupper(L'A'));
    EXPECT_TRUE(iswupper(L'E'));
    EXPECT_TRUE(iswupper(L'Z'));
}
