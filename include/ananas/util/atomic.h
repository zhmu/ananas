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
    enum class memory_order { relaxed, consume, acquire, release, acq_rel, seq_cst };

    namespace detail
    {
        constexpr int to_atomic_memorder(const memory_order mo)
        {
            switch (mo) {
                case memory_order::relaxed:
                    return __ATOMIC_RELAXED;
                case memory_order::consume:
                    return __ATOMIC_CONSUME;
                case memory_order::acquire:
                    return __ATOMIC_ACQUIRE;
                case memory_order::release:
                    return __ATOMIC_RELEASE;
                case memory_order::acq_rel:
                    return __ATOMIC_ACQ_REL;
                case memory_order::seq_cst:
                    return __ATOMIC_SEQ_CST;
                default:
                    // static_assert(0, "memory order constraint not valid");
                    return -1;
            }
        }

        constexpr memory_order get_default_memory_order()
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
            __atomic_store(&a_value, &value, detail::to_atomic_memorder(mo));
        }

        T operator=(T value) noexcept
        {
            store(value);
            return value;
        }

        T load(memory_order mo = detail::get_default_memory_order()) const noexcept
        {
            T value;
            __atomic_load(&a_value, &value, detail::to_atomic_memorder(mo));
            return value;
        }

        operator T() const noexcept { return load(); }

        T fetch_add(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_add(&a_value, arg, detail::to_atomic_memorder(mo));
        }

        T fetch_sub(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_sub(&a_value, arg, detail::to_atomic_memorder(mo));
        }

        T fetch_and(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_and(&a_value, arg, detail::to_atomic_memorder(mo));
        }

        T fetch_or(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_or(&a_value, arg, detail::to_atomic_memorder(mo));
        }

        T fetch_xor(T arg, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            return __atomic_fetch_xor(&a_value, arg, detail::to_atomic_memorder(mo));
        }

        T exchange(T value, memory_order mo = detail::get_default_memory_order()) noexcept
        {
            T prev;
            __atomic_exchange(&a_value, &value, &prev, detail::to_atomic_memorder(mo));
            return prev;
        }

        bool compare_exchange_weak(
            T& expected, T desired, memory_order success, memory_order failure) noexcept
        {
            return __atomic_compare_exchange(
                &a_value, &expected, &desired, true, detail::to_atomic_memorder(success),
                detail::to_atomic_memorder(failure));
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
                &a_value, &expected, &desired, false, detail::to_atomic_memorder(success),
                detail::to_atomic_memorder(failure));
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
