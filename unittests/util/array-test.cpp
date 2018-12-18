#include <gtest/gtest.h>
#include "../../include/ananas/util/array.h"

//#include <array>
// namespace util = std;

TEST(Array, empty)
{
    {
        util::array<int, 0> a;
        EXPECT_TRUE(a.empty());
        EXPECT_EQ(a.end(), a.begin());
    }
    {
        util::array<int, 0> b{};
        EXPECT_TRUE(b.empty());
        EXPECT_EQ(b.end(), b.begin());
    }
}

TEST(Array, at)
{
    util::array<int, 2> a;
    a[0] = 1;
    a[1] = 2;
    EXPECT_EQ(1, a[0]);
    EXPECT_EQ(2, a[1]);
}

TEST(Array, initializer_list)
{
    util::array<int, 4> a{1, 2, 3, 4};
    EXPECT_EQ(4, a.size());
    EXPECT_EQ(1, a[0]);
    EXPECT_EQ(2, a[1]);
    EXPECT_EQ(3, a[2]);
    EXPECT_EQ(4, a[3]);
}

TEST(Array, iteration)
{
    util::array<int, 4> a{1, 2, 3, 4};
    int expected[4] = {1, 2, 3, 4};

    {
        int n = 0;
        for (int v : a) {
            EXPECT_EQ(expected[n], v);
            n++;
        }
        EXPECT_EQ(sizeof(expected) / sizeof(expected[0]), n);
    }

    {
        const auto& b = a;
        int n = 0;
        for (const int v : b) {
            EXPECT_EQ(expected[n], v);
            n++;
        }
        EXPECT_EQ(sizeof(expected) / sizeof(expected[0]), n);
    }
}

TEST(Array, front_back)
{
    util::array<int, 3> a{1, 2, 3};

    EXPECT_EQ(1, a.front());
    EXPECT_EQ(3, a.back());

    const auto& b = a;
    EXPECT_EQ(1, b.front());
    EXPECT_EQ(3, b.back());
}
