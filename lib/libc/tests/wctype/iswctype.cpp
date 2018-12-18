#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswctype)
{
    EXPECT_TRUE(iswctype(L'a', wctype("alpha")));
    EXPECT_TRUE(iswctype(L'z', wctype("alpha")));
    EXPECT_TRUE(iswctype(L'E', wctype("alpha")));
    EXPECT_FALSE(iswctype(L'3', wctype("alpha")));
    EXPECT_FALSE(iswctype(L';', wctype("alpha")));

    EXPECT_TRUE(iswctype(L'a', wctype("alnum")));
    EXPECT_TRUE(iswctype(L'3', wctype("alnum")));
    EXPECT_FALSE(iswctype(L';', wctype("alnum")));

    EXPECT_TRUE(iswctype(L' ', wctype("blank")));
    EXPECT_TRUE(iswctype(L'\t', wctype("blank")));
    EXPECT_FALSE(iswctype(L'\n', wctype("blank")));
    EXPECT_FALSE(iswctype(L';', wctype("blank")));

    EXPECT_TRUE(iswctype(L'\0', wctype("cntrl")));
    EXPECT_TRUE(iswctype(L'\n', wctype("cntrl")));
    EXPECT_TRUE(iswctype(L'\v', wctype("cntrl")));
    EXPECT_TRUE(iswctype(L'\t', wctype("cntrl")));
    EXPECT_FALSE(iswctype(L'a', wctype("cntrl")));

    EXPECT_TRUE(iswctype(L'0', wctype("digit")));
    EXPECT_TRUE(iswctype(L'1', wctype("digit")));
    EXPECT_TRUE(iswctype(L'2', wctype("digit")));
    EXPECT_TRUE(iswctype(L'3', wctype("digit")));
    EXPECT_TRUE(iswctype(L'4', wctype("digit")));
    EXPECT_TRUE(iswctype(L'5', wctype("digit")));
    EXPECT_TRUE(iswctype(L'6', wctype("digit")));
    EXPECT_TRUE(iswctype(L'7', wctype("digit")));
    EXPECT_TRUE(iswctype(L'8', wctype("digit")));
    EXPECT_TRUE(iswctype(L'9', wctype("digit")));
    EXPECT_FALSE(iswctype(L'X', wctype("digit")));
    EXPECT_FALSE(iswctype(L'?', wctype("digit")));

    EXPECT_TRUE(iswctype(L'a', wctype("graph")));
    EXPECT_TRUE(iswctype(L'z', wctype("graph")));
    EXPECT_TRUE(iswctype(L'E', wctype("graph")));
    EXPECT_TRUE(iswctype(L'E', wctype("graph")));
    EXPECT_FALSE(iswctype(L' ', wctype("graph")));
    EXPECT_FALSE(iswctype(L'\t', wctype("graph")));
    EXPECT_FALSE(iswctype(L'\n', wctype("graph")));

    EXPECT_TRUE(iswctype(L'a', wctype("lower")));
    EXPECT_TRUE(iswctype(L'e', wctype("lower")));
    EXPECT_TRUE(iswctype(L'z', wctype("lower")));
    EXPECT_FALSE(iswctype(L'A', wctype("lower")));
    EXPECT_FALSE(iswctype(L'E', wctype("lower")));
    EXPECT_FALSE(iswctype(L'Z', wctype("lower")));

    EXPECT_FALSE(iswctype(L'a', wctype("upper")));
    EXPECT_FALSE(iswctype(L'e', wctype("upper")));
    EXPECT_FALSE(iswctype(L'z', wctype("upper")));
    EXPECT_TRUE(iswctype(L'A', wctype("upper")));
    EXPECT_TRUE(iswctype(L'E', wctype("upper")));
    EXPECT_TRUE(iswctype(L'Z', wctype("upper")));

    EXPECT_TRUE(iswctype(L'Z', wctype("print")));
    EXPECT_TRUE(iswctype(L'a', wctype("print")));
    EXPECT_TRUE(iswctype(L';', wctype("print")));
    EXPECT_FALSE(iswctype(L'\t', wctype("print")));
    EXPECT_FALSE(iswctype(L'\0', wctype("print")));

    EXPECT_TRUE(iswctype(L';', wctype("punct")));
    EXPECT_TRUE(iswctype(L'.', wctype("punct")));
    EXPECT_TRUE(iswctype(L'?', wctype("punct")));
    EXPECT_FALSE(iswctype(L' ', wctype("punct")));
    EXPECT_FALSE(iswctype(L'Z', wctype("punct")));

    EXPECT_TRUE(iswctype(L' ', wctype("space")));
    EXPECT_TRUE(iswctype(L'\t', wctype("space")));

    EXPECT_TRUE(iswctype(L'0', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'1', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'2', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'3', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'4', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'5', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'6', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'7', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'8', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'9', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'a', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'b', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'c', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'd', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'e', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'f', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'A', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'B', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'C', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'D', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'E', wctype("xdigit")));
    EXPECT_TRUE(iswctype(L'F', wctype("xdigit")));
    EXPECT_FALSE(iswctype(L'g', wctype("xdigit")));
    EXPECT_FALSE(iswctype(L'G', wctype("xdigit")));
    EXPECT_FALSE(iswctype(L'x', wctype("xdigit")));
    EXPECT_FALSE(iswctype(L'X', wctype("xdigit")));
    EXPECT_FALSE(iswctype(L' ', wctype("xdigit")));
}
