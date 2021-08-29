/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_UTIL_INTERVAL_MAP_H
#define ANANAS_UTIL_INTERVAL_MAP_H

#include "interval.h"
#include "vector.h"

namespace util
{
    namespace detail {
        template<typename IntervalType, typename ValueType>
        struct entry
        {
            IntervalType interval{};
            ValueType value{};
        };

        template<typename Entry>
        struct iterator
        {
            using value_type = Entry;

            vector<value_type>* i_vector{};
            size_t i_pos{};

            constexpr bool operator==(const iterator& rhs) const { return i_pos == rhs.i_pos && i_vector == rhs.i_vector; }
            constexpr bool operator!=(const iterator& rhs) const { return !(operator==(rhs)); }

            constexpr iterator& operator++() { ++i_pos; return *this; }
            constexpr value_type& operator*() const { return (*i_vector)[i_pos]; }
            constexpr value_type* operator->() const { return &operator*(); }
        };
    }

    template<typename IntervalValueType, typename ValueType>
    struct interval_map {
        using interval_value_type = IntervalValueType;
        using interval_type = util::interval<interval_value_type>;
        using value_type = ValueType;

        using entry = detail::entry<interval_type, value_type>;
        using iterator = detail::iterator<entry>;

        constexpr interval_map() = default;
        interval_map(const interval_map&) = delete;
        interval_map& operator=(const interval_map&) = delete;

        constexpr size_t size() const { return i_entries.size(); }
        constexpr bool empty() const { return size() == 0; }

        constexpr iterator begin() { return iterator{&i_entries, 0}; }
        constexpr iterator end() { return iterator{&i_entries, size()}; }

        void insert(const interval_type& interval, const value_type& value) {
            auto it = i_entries.begin();
            while(it != i_entries.end() && (it->interval) < interval)
                ++it;
            i_entries.insert(it, entry{ interval, util::move(value) });
        }

        constexpr bool remove(const value_type& value) {
            auto it = i_entries.begin();
            while(it != i_entries.end() && it->value != value)
                ++it;
            if (it != i_entries.end()) {
                i_entries.erase(it);
                return true;
            }
            return false;
        }

        constexpr bool remove(const interval_type& interval) {
            auto it = i_entries.begin();
            while(it != i_entries.end() && it->interval != interval)
                ++it;
            if (it != i_entries.end()) {
                i_entries.erase(it);
                return true;
            }
            return false;
        }

        constexpr auto find_interval(const interval_type& interval) {
            auto it = i_entries.begin();
            size_t n = 0;
            while(it != i_entries.end() && it->interval != interval)
                ++it, ++n;
            return iterator{&i_entries, n};
        }

        constexpr auto find_by_value(const interval_value_type& value) {
            auto it = i_entries.begin();
            size_t n = 0;
            while(it != i_entries.end() && !it->interval.contains(value))
                ++it, ++n;
            return iterator{&i_entries, n};
        }

        void clear()
        {
            i_entries.clear();
        }

    private:
        vector<entry> i_entries;
    };

}

#endif
