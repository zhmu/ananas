/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <gtest/gtest.h>
#include "../../include/ananas/util/interval.h"
#include "helpers.h"

using IntervalValueType = short;
using ShortInterval = util::interval<IntervalValueType>;

namespace {
    auto VerifyThatIntervalOfRangeContainsRange(const IntervalValueType from, const IntervalValueType to)
    {
        const ShortInterval interval{ from, to };
        int count{};
        for (const auto n: helpers::every_value<IntervalValueType>()) {
            const auto contained = n >= from && n < to;
            const auto contains = interval.contains(n);
            if (contains) ++count;
            EXPECT_EQ(contained, contains);
        }
        return count;
    }
}

TEST(Interval, DefaultConstructedIsEmpty)
{
    const ShortInterval interval;
    EXPECT_TRUE(interval.empty());
}

TEST(Interval, NothingIsContainedInAnEmptyInterval)
{
    const ShortInterval interval;
    for (const auto n: helpers::every_value<IntervalValueType>()) {
        EXPECT_FALSE(interval.contains(n));
    }
}

TEST(Interval, IntervalContainsValues)
{
    const auto count = VerifyThatIntervalOfRangeContainsRange(10, 20);
    EXPECT_EQ(10, count);
}

TEST(Interval, IntervalOfASingleValueWorks)
{
    const auto count = VerifyThatIntervalOfRangeContainsRange(1, 2);
    EXPECT_EQ(1, count);
}

TEST(Interval, OverlapIsCorrectInAllScenarios)
{
    const ShortInterval a{  10, 100 };
    const ShortInterval b{   0,   7 };
    const ShortInterval c{  80, 170 };
    const ShortInterval d{   8,  15 };
    const ShortInterval e{  30,  50 };
    const ShortInterval f{ 120, 180 };

    EXPECT_EQ(ShortInterval(),        a.overlap(b));
    EXPECT_EQ(ShortInterval(80, 100), a.overlap(c));
    EXPECT_EQ(ShortInterval(10, 15),  a.overlap(d));
    EXPECT_EQ(ShortInterval(30, 50),  a.overlap(e));
    EXPECT_EQ(ShortInterval(),        a.overlap(f));
}
