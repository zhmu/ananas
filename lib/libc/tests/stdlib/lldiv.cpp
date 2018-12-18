#include <gtest/gtest.h>
#include <stdlib.h>

TEST(stdlib, lldiv)
{
    {
        lldiv_t result = lldiv(5ll, 2ll);
        EXPECT_EQ(2, result.quot);
        EXPECT_EQ(1, result.rem);
    }
    {
        lldiv_t result = lldiv(-5ll, 2ll);
        EXPECT_EQ(-2ll, result.quot);
        EXPECT_EQ(-1ll, result.rem);
    }
    {
        lldiv_t result = lldiv(5ll, -2ll);
        EXPECT_EQ(-2, result.quot);
        EXPECT_EQ(1, result.rem);
    }
    {
        lldiv_t result;
        EXPECT_EQ(sizeof(long long), sizeof(result.quot));
        EXPECT_EQ(sizeof(long long), sizeof(result.rem));
    }
}
