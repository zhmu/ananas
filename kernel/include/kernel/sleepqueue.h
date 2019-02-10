/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_SLEEPQUEUE_H
#define ANANAS_SLEEPQUEUE_H

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/spinlock.h"

struct Mutex;
struct SleepQueue;
struct Thread;

namespace sleep_queue
{
    namespace internal
    {
        template<typename T>
        struct ChainNode {
            static typename util::List<T>::Node& Get(T& t) { return t.t_sqchain; }
        };

        template<typename T>
        using ThreadChainNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<ChainNode<T>>;

    } // namespace internal

    struct Waiter {
        Thread* w_thread = nullptr;
        SleepQueue* w_sq = nullptr;
        bool w_signalled = false;
    };

    struct Sleeper;

    typedef util::List<Thread, sleep_queue::internal::ThreadChainNodeAccessor<Thread>>
        SleepQueueThreadList;
} // namespace sleep_queue

struct SleepQueue {
    friend struct sleep_queue::Sleeper;

    SleepQueue(const char* name);
    bool WakeupOne();
    void WakeupAll();

    sleep_queue::Sleeper PrepareToSleep();

    const char* GetName() const { return sq_name; }
    bool HaveSleepers() const;

  protected:
    void Unsleep(register_t state);
    void Sleep(register_t state);

  private:
    const char* sq_name;
    Spinlock sq_lock;
    sleep_queue::SleepQueueThreadList sq_sleepers;
};

namespace sleep_queue
{
    struct Sleeper {
        Sleeper(SleepQueue& sq, register_t state) : sl_sq(sq), sl_state(state) {}

        void Sleep() { sl_sq.Sleep(sl_state); }

        void Cancel() { sl_sq.Unsleep(sl_state); }

      private:
        SleepQueue& sl_sq;
        register_t sl_state;
    };
} // namespace sleep_queue

#endif /* ANANAS_SLEEPQUEUE_H */
