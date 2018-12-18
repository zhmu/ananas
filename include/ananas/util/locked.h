#ifndef ANANAS_UTIL_LOCKED_H
#define ANANAS_UTIL_LOCKED_H

#ifdef KERNEL
#include "kernel/lib.h" // for KASSERT
#endif

namespace util
{
    /*
     * Expresses a currently locked object: if this holds a pointer, that pointer
     * is locked until it is explicitly transferred or unlocked.
     *
     * - Default constructors owns nothing
     * - Other locked<T>'s can be moved, which makes them own nothing
     * - Unlock() must be called (dangling locks are not allowed!)
     * - Extract() can be used to extract the locked object (object will own nothing)
     *
     * Extract() is mainly useful for cleanup scenarios where there is nothing left to check.
     */
    template<typename T>
    class locked
    {
      public:
        locked() = default;
        locked(const locked&) = delete;
        locked& operator=(const locked&) = delete;

        explicit locked(T& obj) noexcept : l_obj(&obj) { l_obj->AssertLocked(); }

        locked(locked&& rhs) noexcept : l_obj(rhs.l_obj)
        {
            l_obj->AssertLocked();
            rhs.l_obj = nullptr;
        }

        locked& operator=(locked&& rhs) noexcept
        {
            KASSERT(!isLocked(), "object still locked");
            l_obj = rhs.l_obj;
            if (l_obj != nullptr)
                l_obj->AssertLocked();
            rhs.l_obj = nullptr;
            return *this;
        }

        ~locked() noexcept { KASSERT(!isLocked(), "object still locked"); }

        T* operator->() const noexcept { return l_obj; }

        T& operator*() const noexcept { return *l_obj; }

        void Unlock() noexcept
        {
            KASSERT(isLocked(), "no object present");
            l_obj->Unlock();
            l_obj = nullptr;
        }

        T* Extract() noexcept
        {
            KASSERT(isLocked(), "no object present");
            T* obj = l_obj;
            l_obj = nullptr;
            return obj;
        }

        operator bool() const noexcept { return isLocked(); }

      private:
        T* l_obj = nullptr;

        bool isLocked() const { return l_obj != nullptr; }
    };

} // namespace util

#endif // ANANAS_UTIL_LOCKED_H
