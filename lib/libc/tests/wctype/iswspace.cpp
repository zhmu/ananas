#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswspace)
{
    EXPECT_TRUE(iswspace(L' '));
    EXPECT_TRUE(iswspace(L'\t'));
    EXPECT_FALSE(iswspace(L'a'));
}
