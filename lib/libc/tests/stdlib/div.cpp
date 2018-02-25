#include <gtest/gtest.h>
#include <stdlib.h>

TEST(stdlib, div)
{
    {
        div_t result = div( 5, 2 );
        EXPECT_EQ(2, result.quot);
        EXPECT_EQ(1, result.rem);
    }
    {
        div_t result = div( -5, 2 );
        EXPECT_EQ(-2, result.quot);
        EXPECT_EQ(-1, result.rem);
    }
    {
        div_t result = div( 5, -2 );
        EXPECT_EQ(-2, result.quot);
        EXPECT_EQ(1, result.rem);
    }
    {
        div_t result;
        EXPECT_EQ(sizeof(int), sizeof(result.quot));
        EXPECT_EQ(sizeof(int), sizeof(result.rem));
    }
}
