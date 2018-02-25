#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswblank)
{
    EXPECT_TRUE(iswblank(L' '));
    EXPECT_TRUE(iswblank(L'\t'));
    EXPECT_FALSE(iswblank(L'\n'));
    EXPECT_FALSE(iswblank(L'a'));
}
