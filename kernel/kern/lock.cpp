/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/pcpu.h"
#include "kernel/schedule.h"
#include "kernel/thread.h"
#include "kernel-md/interrupts.h"

namespace
{
    namespace spinlock
    {
        constexpr inline int destroyed = -1;
        constexpr inline int locked = 1;
        constexpr inline int unlocked = 0;
    } // namespace spinlock

    namespace mutex
    {
        auto destroyed = reinterpret_cast<Thread*>(reinterpret_cast<void*>(-1ull));
        auto unlocking = reinterpret_cast<Thread*>(reinterpret_cast<void*>(-2ull));
        auto unlocked = static_cast<Thread*>(nullptr);
    }
} // unnamed namespace

void Spinlock::Lock()
{
    //if (scheduler::IsActive())
        //KASSERT(md::interrupts::Save(), "interrupts must be enabled");

    while (true) {
        int expected = spinlock::unlocked;
        if (sl_var.compare_exchange_weak(expected, spinlock::locked))
            return;
    }
}

void Spinlock::Unlock()
{
    if (sl_var.exchange(spinlock::unlocked) != spinlock::locked)
        panic("spinlock %p was not locked", this);
}

Spinlock::Spinlock() : sl_var(spinlock::unlocked) {}

Spinlock::~Spinlock()
{
    KASSERT(sl_var.load() == spinlock::unlocked, "destroying corrupted spinlock %p (%x)", this, sl_var.load());
    sl_var = spinlock::destroyed;
}

register_t Spinlock::LockUnpremptible()
{
    register_t state = md::interrupts::Save();

    for (;;) {
        // Spin until unlocked
        while (true) {
            auto val = sl_var.load();
            if (val == spinlock::unlocked) break;
            KASSERT(val == spinlock::locked, "spinlock %p corrupt (%x)", this, val);
        }
        md::interrupts::Disable();
        int expected = spinlock::unlocked;
        if (sl_var.compare_exchange_weak(expected, spinlock::locked))
            break;
        /* Lock failed; restore interrupts and try again */
        md::interrupts::Restore(state);
    }
    return state;
}

void Spinlock::UnlockUnpremptible(register_t state)
{
    Unlock();
    md::interrupts::Restore(state);
}

void Spinlock::AssertLocked()
{
    if (sl_var != spinlock::locked)
        panic("spinlock not held by current thread");
}

void Spinlock::AssertUnlocked()
{
    if (sl_var != spinlock::unlocked)
        panic("spinlock held");
}

Mutex::Mutex(const char* name) : Lockable(name), mtx_owner{ mutex::unlocked } {}

Mutex::~Mutex()
{
    auto expected{mutex::unlocked};
    if (mtx_owner.compare_exchange_strong(expected, mutex::destroyed)) return;

    panic("destroying corrupted mutex %p (%p)", this, expected);
}

void Mutex::Lock()
{
    // Fast path
    if (TryLock()) return;

    // Slow path
    while(true) {
        auto sleeper = mtx_sleepq.PrepareToSleep(*this);
        if (TryLock()) {
            sleeper.Cancel();
            return;
        }

        auto curThread = &thread::GetCurrent();
        if (mtx_owner == curThread) panic("deadlock detected");
        sleeper.Sleep();
    }
}

bool Mutex::TryLock()
{
    auto curThread = &thread::GetCurrent();
    Thread* expected{nullptr};
    if (mtx_owner.compare_exchange_strong(expected, curThread)) return true;
    KASSERT(expected != mutex::destroyed, "trying to lock destroyed mutex %p", this);
    return false;
}

void Mutex::Unlock()
{
    DisableInterruptGuard ig;

    auto curThread = &thread::GetCurrent();
    Thread* value{curThread};
    if (mtx_owner.compare_exchange_strong(value, mutex::unlocking)) {
        mtx_sleepq.WakeupOne(ig);
        mtx_owner = mutex::unlocked;
        return;
    }

    panic("unlocking mutex %p which is owned by %p, not us", this, value);
}

void Mutex::AssertLocked()
{
    auto curThread = &thread::GetCurrent();
    Thread* value{curThread};
    if (!mtx_owner.compare_exchange_strong(value, curThread)) {
        panic("mutex '%s' not held by current thread, but %p", l_name, value);
    }
}

Semaphore::Semaphore(const char* name, int count) : Lockable(name), sem_count(count)
{
    KASSERT(count >= 0, "creating semaphore with negative count %d", count);
}

void Semaphore::Signal()
{
    ++sem_count;

    DisableInterruptGuard ig;
    sem_sleepq.WakeupOne(ig);
}

void Semaphore::Wait()
{
    KASSERT(PCPU_GET(nested_irq) == 0, "waiting in irq");

    // Fast path
    if (TryWait())
        return;

    // Slow path
    while(true) {
        auto sleeper = sem_sleepq.PrepareToSleep(*this);
        if (TryWait()) {
            sleeper.Cancel();
            return;
        }
        sleeper.Sleep();
    }
}

void Semaphore::WaitAndDrain()
{
    Wait();
    sem_count = 0; // XXX racy
}

bool Semaphore::TryWait()
{
    int expected_count = sem_count;
    int new_count = expected_count - 1;
    if (new_count < 0)
        return false; // no more units
    return sem_count.compare_exchange_strong(expected_count, new_count);
}
