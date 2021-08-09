/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
namespace helpers {
    template<typename T> struct values_from_up_to_including
    {
        struct iterator {
            T limit{};
            T current{};
            bool last{};

            bool operator==(const iterator&) const = default;

            constexpr auto& operator++() {
                if (current == limit) {
                    last = true;
                } else {
                    ++current;
                }
                return *this;
            }
            constexpr T operator*() const { return current; }
        };

        const T from;
        const T to;

        constexpr auto begin() const { return iterator{ to, from, false }; };
        constexpr auto end() const { return iterator{ to, to, true }; };
    };

    template<typename T> struct every_value : values_from_up_to_including<T>
    {
        every_value() : values_from_up_to_including<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()) { }
    };
}
