#include <gtest/gtest.h>
#include <stdlib.h>

TEST(stdlib, ldiv)
{
    {
        ldiv_t result = ldiv(5, 2);
        EXPECT_EQ(2, result.quot);
        EXPECT_EQ(1, result.rem);
    }
    {
        ldiv_t result = ldiv(-5, 2);
        EXPECT_EQ(-2, result.quot);
        EXPECT_EQ(-1, result.rem);
    }
    {
        ldiv_t result = ldiv(5, -2);
        EXPECT_EQ(-2, result.quot);
        EXPECT_EQ(1, result.rem);
    }
    {
        ldiv_t result;
        EXPECT_EQ(sizeof(long), sizeof(result.quot));
        EXPECT_EQ(sizeof(long), sizeof(result.rem));
    }
}
