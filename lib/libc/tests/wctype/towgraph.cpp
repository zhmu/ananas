#include <gtest/gtest.h>
#include <wctype.h>

TEST(wctype, towgraph)
{
    EXPECT_TRUE(iswgraph(L'a'));
    EXPECT_TRUE(iswgraph(L'z'));
    EXPECT_TRUE(iswgraph(L'E'));
    EXPECT_FALSE(iswgraph(L' '));
    EXPECT_FALSE(iswgraph(L'\t'));
    EXPECT_FALSE(iswgraph(L'\n'));
}
