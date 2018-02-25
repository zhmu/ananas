#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswdigit)
{
    EXPECT_TRUE(iswdigit(L'0'));
    EXPECT_TRUE(iswdigit(L'1'));
    EXPECT_TRUE(iswdigit(L'2'));
    EXPECT_TRUE(iswdigit(L'3'));
    EXPECT_TRUE(iswdigit(L'4'));
    EXPECT_TRUE(iswdigit(L'5'));
    EXPECT_TRUE(iswdigit(L'6'));
    EXPECT_TRUE(iswdigit(L'7'));
    EXPECT_TRUE(iswdigit(L'8'));
    EXPECT_TRUE(iswdigit(L'9'));
    EXPECT_FALSE(iswdigit(L'a'));
    EXPECT_FALSE(iswdigit(L'b'));
    EXPECT_FALSE(iswdigit(L'c'));
    EXPECT_FALSE(iswdigit(L'd'));
    EXPECT_FALSE(iswdigit(L'e'));
    EXPECT_FALSE(iswdigit(L'f'));
    EXPECT_FALSE(iswdigit(L'A'));
    EXPECT_FALSE(iswdigit(L'B'));
    EXPECT_FALSE(iswdigit(L'C'));
    EXPECT_FALSE(iswdigit(L'D'));
    EXPECT_FALSE(iswdigit(L'E'));
    EXPECT_FALSE(iswdigit(L'F'));
    EXPECT_FALSE(iswdigit(L'g'));
    EXPECT_FALSE(iswdigit(L'G'));
    EXPECT_FALSE(iswdigit(L'x'));
    EXPECT_FALSE(iswdigit(L'X'));
    EXPECT_FALSE(iswdigit(L' '));
}
