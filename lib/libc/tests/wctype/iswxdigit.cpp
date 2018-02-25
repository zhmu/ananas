#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswxdigit)
{
    EXPECT_TRUE(iswxdigit(L'0'));
    EXPECT_TRUE(iswxdigit(L'1'));
    EXPECT_TRUE(iswxdigit(L'2'));
    EXPECT_TRUE(iswxdigit(L'3'));
    EXPECT_TRUE(iswxdigit(L'4'));
    EXPECT_TRUE(iswxdigit(L'5'));
    EXPECT_TRUE(iswxdigit(L'6'));
    EXPECT_TRUE(iswxdigit(L'7'));
    EXPECT_TRUE(iswxdigit(L'8'));
    EXPECT_TRUE(iswxdigit(L'9'));
    EXPECT_TRUE(iswxdigit(L'a'));
    EXPECT_TRUE(iswxdigit(L'b'));
    EXPECT_TRUE(iswxdigit(L'c'));
    EXPECT_TRUE(iswxdigit(L'd'));
    EXPECT_TRUE(iswxdigit(L'e'));
    EXPECT_TRUE(iswxdigit(L'f'));
    EXPECT_TRUE(iswxdigit(L'A'));
    EXPECT_TRUE(iswxdigit(L'B'));
    EXPECT_TRUE(iswxdigit(L'C'));
    EXPECT_TRUE(iswxdigit(L'D'));
    EXPECT_TRUE(iswxdigit(L'E'));
    EXPECT_TRUE(iswxdigit(L'F'));
    EXPECT_FALSE(iswxdigit(L'g'));
    EXPECT_FALSE(iswxdigit(L'G'));
    EXPECT_FALSE(iswxdigit(L'x'));
    EXPECT_FALSE(iswxdigit(L'X'));
    EXPECT_FALSE(iswxdigit(L' '));
}
