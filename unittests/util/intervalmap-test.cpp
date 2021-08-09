/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <gtest/gtest.h>
#include "../../include/ananas/util/interval_map.h"
#include "helpers.h"

namespace {
    using IntervalValueType = short;
    using Interval = util::interval<IntervalValueType>;

    struct Sentinel {
        int value{};

        friend bool operator==(const Sentinel& a, const Sentinel& b) {
            return a.value == b.value;
        }
    };

    constexpr Interval completeInterval{
        std::numeric_limits<IntervalValueType>::min(),
        std::numeric_limits<IntervalValueType>::max()
    };

    constexpr Interval firstInterval{0, 10};
    constexpr Sentinel firstSentinel{1};

    constexpr Interval secondInterval{20, 5};
    constexpr Sentinel secondSentinel{2};

    constexpr auto AllValuesOfInterval(const Interval& interval) {
        return helpers::values_from_up_to_including(interval.begin, static_cast<Interval::value_type>(interval.end - 1));
    }
}

using IntervalMap = util::interval_map<IntervalValueType, Sentinel>;

TEST(IntervalMap, InitialMapIsEmpty)
{
    IntervalMap map;
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(0, map.size());
}

TEST(IntervalMap, EmptyMapHasEqualBeginAndEndIterators)
{
    IntervalMap map;
    EXPECT_EQ(map.begin(), map.end());
}

TEST(IntervalMap, InsertSingleItem)
{
    IntervalMap map;
    map.insert(firstInterval, firstSentinel);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(1, map.size());

    size_t count{};
    for(const auto& [ interval, value ]: map) {
        EXPECT_EQ(interval, firstInterval);
        EXPECT_EQ(value, firstSentinel);
        ++count;
    }
    EXPECT_EQ(1, count);
}

TEST(IntervalMap, InsertTwoItemsMaintainsOrdering)
{
    IntervalMap map;
    map.insert(secondInterval, secondSentinel);
    map.insert(firstInterval, firstSentinel);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(2, map.size());

    size_t count{};
    for(const auto& [ interval, value ]: map) {
        if (count == 0) {
            EXPECT_EQ(interval, firstInterval);
            EXPECT_EQ(value, firstSentinel);
        } else if (count == 1) {
            EXPECT_EQ(interval, secondInterval);
            EXPECT_EQ(value, secondSentinel);
        }
        ++count;
    }
    EXPECT_EQ(2, count);
}

TEST(IntervalMap, RemoveDoesNothingOnEmptyMap)
{
    IntervalMap map;
    map.remove(firstInterval);
    map.remove(firstSentinel);
    EXPECT_TRUE(map.empty());
}

TEST(IntervalMap, RemoveDoesNothingIfTheItemDoesNotExist)
{
    IntervalMap map;
    map.insert(firstInterval, firstSentinel);
    map.remove(secondInterval);
    map.remove(secondSentinel);
    EXPECT_FALSE(map.empty());

    size_t count{};
    for(const auto& [ interval, value ]: map) {
        EXPECT_EQ(interval, firstInterval);
        EXPECT_EQ(value, firstSentinel);
        ++count;
    }
    EXPECT_EQ(1, count);
}

TEST(IntervalMap, RemoveRemovesWhenItWasPresent)
{
    IntervalMap map;
    map.insert(firstInterval, firstSentinel);
    map.insert(secondInterval, secondSentinel);
    map.remove(firstInterval);
    ASSERT_FALSE(map.empty());
    EXPECT_EQ(secondInterval, map.begin()->interval);
    EXPECT_EQ(secondSentinel, map.begin()->value);
    map.remove(secondSentinel);
    EXPECT_TRUE(map.empty());
}

TEST(IntervalMap, FindReturnsNothingIfEmpty)
{
    IntervalMap map;
    EXPECT_EQ(map.end(), map.find_by_value(0));
}

TEST(IntervalMap, FindFindsValuesInsideInterval)
{
    IntervalMap map;
    map.insert(firstInterval, firstSentinel);
    for(const auto n: AllValuesOfInterval(firstInterval)) {
        auto it = map.find_by_value(n);
        ASSERT_NE(map.end(), it);
        EXPECT_EQ(it->interval, firstInterval);
        EXPECT_EQ(it->value, firstSentinel);
    }
}

TEST(IntervalMap, FindDoesNotFindAnythingOutsideTheInterval)
{
    IntervalMap map;
    map.insert(firstInterval, firstSentinel);
    for(const auto n: helpers::every_value<IntervalValueType>()) {
        if (firstInterval.contains(n)) continue;
        auto it = map.find_by_value(n);
        EXPECT_EQ(map.end(), it);
    }
}

TEST(IntervalMap, FindFindsValuesGivenMultipleIntervals)
{
    IntervalMap map;
    map.insert(firstInterval, firstSentinel);
    map.insert(secondInterval, secondSentinel);
    for(const auto n: helpers::every_value<IntervalValueType>()) {
        const auto it = map.find_by_value(n);
        if (firstInterval.contains(n)) {
            ASSERT_NE(map.end(), it);
            EXPECT_EQ(it->value, firstSentinel);
        } else if (secondInterval.contains(n)) {
            ASSERT_NE(map.end(), it);
            EXPECT_EQ(it->value, secondSentinel);
        } else {
            EXPECT_EQ(map.end(), it);
        }
    }
}

TEST(IntervalMap, FindWithIntervalFindsExactMatches)
{
    IntervalMap map;
    map.insert(firstInterval, firstSentinel);
    map.insert(secondInterval, secondSentinel);

    const auto it1 = map.find_interval(firstInterval);
    ASSERT_NE(map.end(), it1);
    EXPECT_EQ(it1->value, firstSentinel);

    const auto it2 = map.find_interval(secondInterval);
    ASSERT_NE(map.end(), it2);
    EXPECT_EQ(it2->value, secondSentinel);
}

TEST(IntervalMap, FindWithIntervalDoesNotYieldPartialMatches)
{
    IntervalMap map;
    map.insert(firstInterval, firstSentinel);

    const auto it1 = map.find_interval(Interval{ firstInterval.begin, firstInterval.end - 1 } );
    EXPECT_EQ(map.end(), it1);
    const auto it2 = map.find_interval(Interval{ firstInterval.begin + 1, firstInterval.end } );
    EXPECT_EQ(map.end(), it2);
}

TEST(IntervalMap, FindWithIntervalDoesNotYieldOverlappingMatches)
{
    IntervalMap map;
    map.insert(firstInterval, firstSentinel);

    const auto it = map.find_interval(completeInterval);
    EXPECT_EQ(map.end(), it);
}
