#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, wctrans)
{
    EXPECT_EQ(0, wctrans(""));
    EXPECT_EQ(0, wctrans("invalid"));
    EXPECT_NE((wctrans_t)0, wctrans("toupper"));
    EXPECT_NE((wctrans_t)0, wctrans("tolower"));
}
