/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <machine/param.h>
#include "kernel/console.h"
#include "kernel/device.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel-md/pit.h"
#include "kernel-md/interrupts.h"
#include "options.h" // for ARCHITECTURE

/* If set, display the entire init tree before launching it */
#define VERBOSE_INIT 0

namespace init
{
    namespace
    {
        util::List<OnInit> initFunctions;
    } // unnamed namespace

    namespace internal
    {
        void Register(OnInit& onInit) { initFunctions.push_back(onInit); }
    } // namespace internal

    namespace
    {
        void run_init()
        {
            using namespace detail;

            // Count the number of init functions and make a pointer list of them; we'll
            // sort this pointer list
            const size_t initFunctionCount = []() {
                size_t n = 0;
                for (auto& ifn : initFunctions) {
                    n++;
                }
                return n;
            }();

            auto initFunctionChain = new OnInit*[initFunctionCount + 1];
            {
                size_t n = 0;
                for (auto& ifn : initFunctions) {
                    initFunctionChain[n] = &ifn;
                    n++;
                }
                initFunctionChain[n] = nullptr;
            }

            // Sort the init functions chain; we use a simple bubble sort to do so
            for (int i = 0; i < initFunctionCount; i++)
                for (int j = initFunctionCount - 1; j > i; j--) {
                    auto a = initFunctionChain[j];
                    auto b = initFunctionChain[j - 1];
                    if ((a->oi_subsystem > b->oi_subsystem) ||
                        (a->oi_subsystem >= b->oi_subsystem && a->oi_order >= b->oi_order))
                        continue;
                    initFunctionChain[j] = b;
                    initFunctionChain[j - 1] = a;
                }

#if VERBOSE_INIT
            kprintf("Init tree\n");
            for (int n = 0; n < initFunctionCount; n++) {
                const auto& ifn = *initFunctionChain[n];
                kprintf(
                    "initfunc %u -> %p (subsys %x, order %x)\n", n, ifn.oi_func,
                    static_cast<int>(ifn.oi_subsystem), static_cast<int>(ifn.oi_order));
            }
#endif

            /* Execute all init functions in order except the final one */
            for (int n = 0; n < initFunctionCount - 1; n++)
                initFunctionChain[n]->oi_func();

            /* Throw away the init function chain; it served its purpose */
            auto finalInitFuntion = initFunctionChain[initFunctionCount - 1];
            delete[] initFunctionChain;

            /* Call the final init function; it shouldn't return */
            finalInitFuntion->oi_func();
        }

        const init::OnInit helloWorld(init::SubSystem::Console, init::Order::Last, []() {
            /* Show a startup banner */
            kprintf("Ananas/%s - %s %s\n", ARCHITECTURE, __DATE__, __TIME__);
            unsigned int total_pages, avail_pages;
            page_get_stats(&total_pages, &avail_pages);
            kprintf(
                "Memory: %uKB available / %uKB total\n", avail_pages * (PAGE_SIZE / 1024),
                total_pages * (PAGE_SIZE / 1024));
            kprintf("CPU: %u MHz\n", x86_get_cpu_frequency());
        });

        void init_thread_func(void* done)
        {
            run_init();

            /* All done - signal and exit - the reaper will clean up this thread */
            *(volatile int*)done = 1;
            thread_exit(0);
            /* NOTREACHED */
        }

    } // unnamed namespace
} // namespace init

void mi_startup()
{
    // Create a thread to actually perform initialisation - mi_startup() will eventually
    // become the idle thread and must thus never sleep
    volatile int done = 0;
    Thread init_thread;
    kthread_init(init_thread, "init", &init::init_thread_func, (void*)&done);
    init_thread.Resume();

    /* Activate the scheduler - it is time */
    scheduler::Launch();

    /*
     * Okay, for time being this will be the idle thread - we must not sleep, as
     * we are the idle thread
     */
    while (!done) {
        md::interrupts::Relax();
    }

    /* And now, we become the idle thread */
    idle_thread(nullptr);

    /* NOTREACHED */
}
