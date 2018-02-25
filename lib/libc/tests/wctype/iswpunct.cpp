#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, iswpunct)
{
    EXPECT_TRUE(iswpunct(L';'));
    EXPECT_TRUE(iswpunct(L'?'));
    EXPECT_TRUE(iswpunct(L'.'));
    EXPECT_FALSE(iswpunct(L' '));
    EXPECT_FALSE(iswpunct(L'Z'));
}
