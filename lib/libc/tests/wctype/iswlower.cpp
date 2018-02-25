#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswlower)
{
    EXPECT_TRUE(iswlower(L'a'));
    EXPECT_TRUE(iswlower(L'e'));
    EXPECT_TRUE(iswlower(L'z'));
    EXPECT_FALSE(iswlower(L'A'));
    EXPECT_FALSE(iswlower(L'E'));
    EXPECT_FALSE(iswlower(L'Z'));
}
