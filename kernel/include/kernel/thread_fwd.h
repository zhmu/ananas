/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __THREAD_FWD_H__
#define __THREAD_FWD_H__

#include <ananas/types.h>
#include <ananas/util/list.h>

struct Thread;

namespace thread {
    // Internal stuff so we can work with children and all nodes
    namespace internal
    {
        template<typename T>
        struct AllNode {
            static typename util::List<T>::Node& Get(T& t) { return t.t_NodeAllThreads; }
        };

        template<typename T>
        struct SchedulerNode {
            static typename util::List<T>::Node& Get(T& t) { return t.t_NodeSchedulerList; }
        };

        template<typename T>
        struct SleepQueueChainNode {
            static typename util::List<T>::Node& Get(T& t) { return t.t_sqchain; }
        };

        template<typename T>
        using ThreadAllNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<AllNode<T>>;
        template<typename T>
        using ThreadSchedulerNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<SchedulerNode<T>>;

        template<typename T>
        using ThreadSleepQueueChainNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<SleepQueueChainNode<T>>;
    } // namespace internal

    using AllThreadsList = util::List<Thread, thread::internal::ThreadAllNodeAccessor<Thread>>;
    using SchedulerThreadList = util::List<Thread, thread::internal::ThreadSchedulerNodeAccessor<Thread>>;
    using SleepQueueThreadList = util::List<Thread, thread::internal::ThreadSleepQueueChainNodeAccessor<Thread>>;
}


#endif
