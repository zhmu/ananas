/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_UTIL_INTERVAL_H
#define ANANAS_UTIL_INTERVAL_H

namespace util
{
    // Contains the half-open interval [begin, end)
    template<typename T>
    struct interval {
        using value_type = T;

        value_type begin{};
        value_type end{};

        constexpr auto empty() const {
            return begin == end;
        }

        constexpr auto contains(const value_type& value) const {
            return value >= begin && value < end;
        }

        constexpr auto overlap(const interval& other) const {
            interval result;
            if (begin >= other.end) return result;
            if (end <= other.begin) return result;
            result.begin = (begin > other.begin) ? begin : other.begin;
            result.end = (end < other.end) ? end : other.end;
            return result;
        }

        friend constexpr auto operator==(const interval& a, const interval& b) {
            return a.begin == b.begin && a.end == b.end;
        }

        friend constexpr auto operator!=(const interval& a, const interval& b) {
            return !(a == b);
        }

        friend constexpr auto operator<(const interval& a, const interval& b) {
            if (a.begin != b.begin) return a.begin < b.begin;
            if (a.end != b.end) return a.end < b.end;
            return false;
        }
    };
}

#endif
