/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_UTIL_PAIR_H
#define ANANAS_UTIL_PAIR_H

namespace util
{
    template<typename T1, typename T2>
    struct pair {
        constexpr pair() = default;

        pair(const T1& v1, const T2& v2) : first(v1), second(v2) {}

        template<class U1, class U2>
        pair(const pair<U1, U2>& p)
        {
        }

        T1 first{};
        T2 second{};
    };

    template<class T1, typename T2>
    constexpr pair<T1, T2> make_pair(T1&& t, T2&& u)
    {
        return util::pair<T1, T2>(t, u);
    }

} // namespace util

#endif /* ANANAS_UTIL_PAIR_H */
