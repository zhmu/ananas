/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <gtest/gtest.h>

#include "../../include/ananas/util/atomic.h"
//#include <atomic>
// namespace util = std;

// XXX This test should be done with other types than int

TEST(Atomic, construction)
{
    {
        util::atomic<int> a;
        EXPECT_EQ(int{}, a.load());
        EXPECT_EQ(int{}, a);
    }
    {
        util::atomic<int> b(42);
        EXPECT_EQ(42, b.load());
        EXPECT_EQ(42, b);
    }
}

TEST(Atomic, assignment)
{
    util::atomic<int> a;
    a = 13;
    EXPECT_EQ(13, a);
    a = 37;
    EXPECT_EQ(37, a);
}

TEST(Atomic, add)
{
    util::atomic<int> a(10);
    EXPECT_EQ(10, a.fetch_add(5));
    EXPECT_EQ(15, a);
    EXPECT_EQ(15, a.fetch_add(-10));
    EXPECT_EQ(5, a);
    EXPECT_EQ(5, a++);
    EXPECT_EQ(6, a);
    EXPECT_EQ(7, ++a);
    EXPECT_EQ(7, a);
    EXPECT_EQ(17, a += 10);
    EXPECT_EQ(17, a);
}

TEST(Atomic, sub)
{
    util::atomic<int> a(30);
    EXPECT_EQ(30, a.fetch_sub(3));
    EXPECT_EQ(27, a);
    EXPECT_EQ(27, a.fetch_sub(-15));
    EXPECT_EQ(42, a);
    EXPECT_EQ(42, a--);
    EXPECT_EQ(41, a);
    EXPECT_EQ(40, --a);
    EXPECT_EQ(40, a);
    EXPECT_EQ(30, a -= 10);
    EXPECT_EQ(30, a);
}

TEST(Atomic, and)
{
    {
        util::atomic<int> a(0xff);
        EXPECT_EQ(0xff, a.fetch_and(0xf0));
        EXPECT_EQ(0xf0, a);
        EXPECT_EQ(0xf0, a.fetch_and(0x0f));
        EXPECT_EQ(0, a);
    }
    {
        util::atomic<int> a(0x16);
        EXPECT_EQ(6, a &= 0xf);
        EXPECT_EQ(6, a);
    }
}

TEST(Atomic, or)
{
    {
        util::atomic<int> a(0);
        EXPECT_EQ(0, a.fetch_or(0xf0));
        EXPECT_EQ(0xf0, a);
        EXPECT_EQ(0xf0, a.fetch_or(0x0f));
        EXPECT_EQ(0xff, a);
    }
    {
        util::atomic<int> a(0xf);
        EXPECT_EQ(0xff, a |= 0xf0);
        EXPECT_EQ(0xff, a);
    }
}

TEST(Atomic, xor)
{
    util::atomic<int> a(0xff);
    EXPECT_EQ(0xff, a.fetch_xor(0xaa));
    EXPECT_EQ(0x55, a);
    EXPECT_EQ(0x55, a.fetch_xor(0xff));
    EXPECT_EQ(0xaa, a);
    EXPECT_EQ(0xab, a ^= 1);
    EXPECT_EQ(0xab, a);
}

TEST(Atomic, exchange)
{
    util::atomic<int> a{};
    EXPECT_EQ(0, a.exchange(13));
    EXPECT_EQ(13, a.exchange(37));
    EXPECT_EQ(37, a);
}

// XXX this only compares the strong version as the weak version may fail and ought to be retried
TEST(Atomic, compare_exchange)
{
    util::atomic<int> a{};

    int v{};
    EXPECT_TRUE(a.compare_exchange_strong(v, 13));
    EXPECT_EQ(13, a);
    v = 0;
    EXPECT_FALSE(a.compare_exchange_strong(v, 37));
    EXPECT_EQ(13, a);
}
