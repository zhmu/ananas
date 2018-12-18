#ifndef ANANAS_UTIL_REFCOUNTED_H
#define ANANAS_UTIL_REFCOUNTED_H

#ifdef KERNEL
#include "kernel/lib.h" // for KASSERT
#endif
#include "atomic.h"

namespace util
{
    namespace detail
    {
        // Default cleanup just delete's the object
        template<typename T>
        struct default_refcounted_cleanup {
            static void Cleanup(T* obj) { delete obj; }
        };

    } // namespace detail

    template<typename T, typename Cleanup = detail::default_refcounted_cleanup<T>>
    class refcounted
    {
      public:
        void AddReference() { ++rc_refcount; }

        void RemoveReference()
        {
            if (--rc_refcount > 0)
                return;

            Cleanup::Cleanup(static_cast<T*>(this));
        }

        // HACK This is only for DEBUGGING / TESTING use
        int GetReferenceCount() const { return rc_refcount; }

      protected:
        refcounted() = default;
        ~refcounted()
        {
            KASSERT(rc_refcount == 0, "destroyed while still %d refs", GetReferenceCount());
        }

        // Prevent copying and assignment
        refcounted(const refcounted&) = delete;
        refcounted& operator=(const refcounted&) = delete;

      private:
        util::atomic<int> rc_refcount{1}; // caller gets one ref
    };

} // namespace util
#endif // ANANAS_UTIL_REFCOUNTED_H
