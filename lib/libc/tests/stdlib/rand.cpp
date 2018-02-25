#include <gtest/gtest.h>
#include <stdlib.h>

TEST(stdlib, rand)
{
    int rnd1 = rand();
    EXPECT_LE(rnd1, RAND_MAX);
    int rnd2 = rand();
    EXPECT_LE(rnd2, RAND_MAX);
    srand(1);
    EXPECT_EQ(rnd1, rand());
    EXPECT_EQ(rnd2, rand());
}
