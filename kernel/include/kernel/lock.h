/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"
#include <ananas/util/atomic.h>
#include <ananas/util/list.h>
#include "kernel/sleepqueue.h"
#include "kernel/spinlock.h"
#include "kernel-md/interrupts.h"

struct Thread;

namespace detail
{
    template<typename T>
    class LockGuard final
    {
      public:
        LockGuard(T& lock) : lg_Lock(lock) { lg_Lock.Lock(); }

        ~LockGuard() { lg_Lock.Unlock(); }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

      private:
        T& lg_Lock;
    };

} // namespace detail

class Lockable
{
public:
    Lockable(const char* name) : l_name(name) { }

    const char* l_name;
};

// Simple guard for the spinlock
class SpinlockUnpremptibleGuard final
{
  public:
    SpinlockUnpremptibleGuard(Spinlock& lock) : sug_Lock(lock)
    {
        sug_State = sug_Lock.LockUnpremptible();
    }
    ~SpinlockUnpremptibleGuard() { sug_Lock.UnlockUnpremptible(sug_State); }

    SpinlockUnpremptibleGuard(const SpinlockUnpremptibleGuard&) = delete;
    SpinlockUnpremptibleGuard& operator=(const SpinlockUnpremptibleGuard&) = delete;

  private:
    Spinlock& sug_Lock;
    register_t sug_State;
};

/*
 * Semaphores are sleepable locks which guard an amount of units of a
 * particular resource.
 */
class Semaphore final : public Lockable
{
  public:
    Semaphore(const char* name, int count);
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    void Signal();
    void Wait();
    bool TryWait();
    void WaitAndDrain();

  private:
    util::atomic<int> sem_count;
    SleepQueue sem_sleepq;
};

/*
 * Mutexes are sleepable locks that will suspend the current thread when the
 * lock is already being held. They cannot be used from interrupt context.
 */
struct Mutex final : public Lockable {
  public:
    Mutex(const char* name);
    ~Mutex();
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void Lock();
    void Unlock();

    bool TryLock();
    void AssertLocked();
    void AssertUnlocked();

  private:
    util::atomic<Thread*> mtx_owner;
    SleepQueue mtx_sleepq;
};

// Ensures interrupts are disabled while in scope
class DisableInterruptGuard {
    register_t state;

public:
    DisableInterruptGuard()
        : state(md::interrupts::Save())
    {
        md::interrupts::Disable();
    }

    ~DisableInterruptGuard()
    {
        md::interrupts::Restore(state);
    }

    DisableInterruptGuard(const DisableInterruptGuard&) = delete;
    DisableInterruptGuard& operator=(const DisableInterruptGuard&) = delete;
};

using SpinlockGuard = detail::LockGuard<Spinlock>;
using MutexGuard = detail::LockGuard<Mutex>;
