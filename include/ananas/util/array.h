/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_UTIL_ARRAY_H
#define ANANAS_UTIL_ARRAY_H

namespace util
{
    namespace detail
    {
        template<typename T>
        struct base_array_iterator {
            base_array_iterator(T* p) : ptr(p) {}

            base_array_iterator& operator++()
            {
                ++ptr;
                return *this;
            }

            base_array_iterator operator++(int)
            {
                auto s(*this);
                ++ptr;
                return s;
            }

            base_array_iterator& operator--()
            {
                --ptr;
                return *this;
            }

            base_array_iterator operator--(int)
            {
                auto s(*this);
                --ptr;
                return s;
            }

            T* operator->() const { return ptr; }

            T& operator*() const { return *ptr; }

            bool operator==(const base_array_iterator& rhs) const { return ptr == rhs.ptr; }

            bool operator!=(const base_array_iterator& rhs) const { return !operator==(rhs); }

          private:
            T* ptr;
        };

    } // namespace detail

    // Similar to std::array - fixed-length array container
    template<typename T, size_t N>
    struct array {
        using iterator = detail::base_array_iterator<T>;
        using const_iterator = detail::base_array_iterator<const T>;

        constexpr bool empty() const { return N == 0; }

        constexpr size_t size() const { return N; }

        constexpr T& operator[](size_t n) { return __data[n]; }

        constexpr const T& operator[](size_t n) const { return __data[n]; }

        T& front() { return __data[0]; }

        const T& front() const { return __data[0]; }

        T& back() { return __data[N - 1]; }

        const T& back() const { return __data[N - 1]; }

        iterator begin() { return iterator(&__data[0]); }

        iterator end() { return iterator(&__data[N]); }

        const_iterator begin() const { return const_iterator(&__data[0]); }

        const_iterator end() const { return const_iterator(&__data[N]); }

        T __data[N];
    };

} // namespace util

#endif /* ANANAS_UTIL_ARRAY_H */
