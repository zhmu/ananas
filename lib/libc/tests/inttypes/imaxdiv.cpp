#include <gtest/gtest.h>
#include <inttypes.h>

TEST(inttypes, imaxdiv)
{
    {
        imaxdiv_t result = imaxdiv((intmax_t)5, (intmax_t)2);
        EXPECT_EQ(2, result.quot);
        EXPECT_EQ(1, result.rem);
    }
    {
        imaxdiv_t result = imaxdiv((intmax_t)-5, (intmax_t)2);
        EXPECT_EQ(-2, result.quot);
        EXPECT_EQ(-1, result.rem);
    }
    {
        imaxdiv_t result = imaxdiv((intmax_t)5, (intmax_t)-2);
        EXPECT_EQ(-2, result.quot);
        EXPECT_EQ(1, result.rem);
    }
    {
        imaxdiv_t result;
        EXPECT_EQ(sizeof(intmax_t), sizeof(result.quot));
        EXPECT_EQ(sizeof(intmax_t), sizeof(result.rem));
    }
}
