#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, towctrans)
{
    EXPECT_EQ(L'A', towctrans(L'a', wctrans("toupper")));
    EXPECT_EQ(L'B', towctrans(L'B', wctrans("toupper")));
    EXPECT_EQ(L'a', towctrans(L'a', wctrans("tolower")));
    EXPECT_EQ(L'b', towctrans(L'B', wctrans("tolower")));
    EXPECT_EQ(L'B', towctrans(L'B', wctrans("invalid")));
    EXPECT_EQ(L'B', towctrans(L'B', 0));
}
