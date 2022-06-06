/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/util/atomic.h>
#include "kernel/gdb.h"
#include "kernel/lib.h"
#include "kernel/schedule.h"
#include "kernel-md/smp.h" // XXX
#include "kernel-md/interrupts.h"

extern "C" void* __dso_handle;
void* __dso_handle = nullptr;

namespace
{
    constexpr addr_t kdb_no_panic = 0;
    util::atomic<addr_t> kdb_panicing{kdb_no_panic};
} // namespace

void _panic(const char* file, const char* func, int line, const char* fmt, ...)
{
    va_list ap;
    md::interrupts::Disable();

    /*
     * Ensure our other CPU's do not continue - we do not want to make things
     * worse.
     */
    smp::PanicOthers();

    /*
     * Do our best to detect double panics; this will at least give more of a
     * clue what's going on instead of making it worse. Note that we use the
     * strong compare/exchange here as we do not want to deal with retries.
     */
    addr_t expected = kdb_no_panic;
    if (!kdb_panicing.compare_exchange_strong(expected, reinterpret_cast<addr_t>(&_panic))) {
        kprintf("double panic: %s:%u (%s) - dying!\n", file, line, func);
        for (;;)
            /* nothing */;
    }

    /* disable the scheduler - this ensures any extra BSP's will not run threads either */
    scheduler::Deactivate();

    kprintf("panic in %s:%u (%s): ", file, line, func);

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    kprintf("\n");

    // Throw it over to GDB
    while(true)
        gdb_enter();
}

extern "C" void __cxa_pure_virtual() { panic("pure virtual call"); }

extern "C" int __cxa_guard_acquire(__int64_t* guard_object)
{
    // XXX We ought to actually lock stuff here
    auto initialized = reinterpret_cast<char*>(guard_object);
    return *initialized == 0;
}

extern "C" void __cxa_guard_release(__int64_t* guard_object)
{
    // XXX We ought to actually lock stuff here
    *guard_object = 0;
}

extern "C" int __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle)
{
    // Unnecessary: we never run global destructors
    return 0;
}
