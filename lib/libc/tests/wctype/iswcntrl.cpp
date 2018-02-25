#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswcntrl)
{
    EXPECT_TRUE(iswcntrl(L'\0'));
    EXPECT_TRUE(iswcntrl(L'\n'));
    EXPECT_TRUE(iswcntrl(L'\v'));
    EXPECT_TRUE(iswcntrl(L'\t'));
    EXPECT_FALSE(iswcntrl(L'a'));
}
