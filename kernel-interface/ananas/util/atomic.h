/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANS_UTIL_ATOMIC_H
#define ANANS_UTIL_ATOMIC_H

namespace util
{
    enum class memory_order {
        relaxed = __ATOMIC_RELAXED,
        consume = __ATOMIC_CONSUME,
        acquire = __ATOMIC_ACQUIRE,
        release = __ATOMIC_RELEASE,
        acq_rel = __ATOMIC_ACQ_REL,
        seq_cst = __ATOMIC_SEQ_CST,
    };

    namespace detail
    {
        constexpr auto get_default_memory_order()
        {
            // Safest, but also slowest
            return memory_order::seq_cst;
        }

    } // namespace detail

    template<typename T>
    class atomic
    {
      public:
        atomic(T value = {}) noexcept { store(value); }

        atomic(const atomic& a) = delete;
        atomic& operator=(const atomic& a) = delete;

        void store(T value, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            __atomic_store(&a_value, &value, static_cast<int>(mo));
        }

        T operator=(T value) noexcept
        {
            store(value);
            return value;
        }

        T load(memory_order mo = detail::get_default_memory_order()) const noexcept
        {
            T value;
            __atomic_load(&a_value, &value, static_cast<int>(mo));
            return value;
        }

        operator T() const noexcept { return load(); }

        T fetch_add(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_add(&a_value, arg, static_cast<int>(mo));
        }

        T fetch_sub(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_sub(&a_value, arg, static_cast<int>(mo));
        }

        T fetch_and(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_and(&a_value, arg, static_cast<int>(mo));
        }

        T fetch_or(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_or(&a_value, arg, static_cast<int>(mo));
        }

        T fetch_xor(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_xor(&a_value, arg, static_cast<int>(mo));
        }

        T exchange(T value, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            T prev;
            __atomic_exchange(&a_value, &value, &prev, static_cast<int>(mo));
            return prev;
        }

        bool compare_exchange_weak(
            T& expected, T desired, memory_order success, memory_order failure) noexcept
        {
            return __atomic_compare_exchange(
                &a_value, &expected, &desired, true,
                static_cast<int>(success),
                static_cast<int>(failure));
        }

        bool compare_exchange_weak(
            T& expected, T desired,
            memory_order order = detail::get_default_memory_order()) noexcept
        {
            return compare_exchange_weak(expected, desired, order, order);
        }

        bool compare_exchange_strong(
            T& expected, T desired, memory_order success, memory_order failure) noexcept
        {
            return __atomic_compare_exchange(
                &a_value, &expected, &desired, false, static_cast<int>(success),
                static_cast<int>(failure));
        }

        bool compare_exchange_strong(
            T& expected, T desired,
            memory_order order = detail::get_default_memory_order()) noexcept
        {
            return compare_exchange_strong(expected, desired, order, order);
        }

        T operator++() noexcept { return fetch_add(T{1}) + T{1}; }

        T operator++(int) noexcept { return fetch_add(T{1}); }

        T operator--() noexcept { return fetch_sub(T{1}) - T{1}; }

        T operator--(int) noexcept { return fetch_sub(T{1}); }

        T operator+=(T arg) noexcept { return fetch_add(arg) + arg; }

        T operator-=(T arg) noexcept { return fetch_sub(arg) - arg; }

        T operator&=(T arg) noexcept { return fetch_and(arg) & arg; }

        T operator|=(T arg) noexcept { return fetch_or(arg) | arg; }

        T operator^=(T arg) noexcept { return fetch_xor(arg) ^ arg; }

      private:
        T a_value;
    };

} // namespace util

#endif // ANANS_UTIL_ATOMIC_H
