/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/irq.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/result.h"
#include "kernel/time.h"
#include "kernel-md/io.h"
#include "kernel-md/pit.h"
#include "kernel-md/interrupts.h"

namespace
{
    inline constexpr int timerFreq = 1193182;
    int cpuFrequency = -1;

    uint64_t tsc_boot_time;

} // unnamed namespace

/*
 * Obtains the number of milliseconds that have passed since boot. Because we
 * know the CPU speed in MHz as cpuFrequency, it means one second means
 * '1000 * cpuFrequency' ticks have passed.
 */
uint32_t x86_get_ms_since_boot()
{
    uint64_t tsc = rdtsc() - tsc_boot_time;
    return (tsc / 1000) / cpuFrequency;
}

int x86_get_cpu_frequency() { return cpuFrequency; }

void x86_pit_calc_cpuspeed_mhz()
{
    // Use PIT timer 2 to wait one tick - this is the only timer we can read the
    // output from, which prevents having to use interrupts
    {
        outb(PIT_KBD_B_CTRL, (inb(PIT_KBD_B_CTRL) & PIT_KBD_B_MASK1) | PIT_KBD_B_T2GATE);
        outb(PIT_MODE_CMD, PIT_CH_CHAN2 | PIT_ACCESS_BOTH);
        const uint16_t count = timerFreq / time::GetPeriodicyInHz();
        outb(PIT_CH2_DATA, (count & 0xff));
        outb(PIT_CH2_DATA, (count >> 8));
    }

    {
        // Pulse T2 gate to enable timer
        auto ctrl = inb(PIT_KBD_B_CTRL) & PIT_KBD_B_MASK2;
        outb(PIT_KBD_B_CTRL, ctrl);
        outb(PIT_KBD_B_CTRL, ctrl | PIT_KBD_B_T2GATE);
    }

    // Now wait until the timer output is active and count the number of cycles
    uint64_t tsc_base = rdtsc();
    while ((inb(PIT_KBD_B_CTRL) & PIT_KBD_B_T2OUTPUT) == 0)
        /* nothing */;
    uint64_t tsc_current = rdtsc();
    // We use the tsc_current value as the boot time
    tsc_boot_time = tsc_current;
    cpuFrequency = ((tsc_current - tsc_base) * time::GetPeriodicyInHz()) / 1000000;
    if (cpuFrequency < 500) {
        panic("unable to properly measure CPU frequency using TSC/PIT");
    }
}

void delay(int ms)
{
    /* XXX For now, always delay using the TSC */

    /*
     * Delaying using the TSC; we should already have initialized cpuFrequency
     * by now; this is the number of MHz the CPU clock is running at; so delaying
     * one second will take 'cpuFrequency* 1000000' ticks. This means
     * delaying one millisecond is 1000 times faster, so we'll just have to
     * wait 'ms * cpuFrequency* 1000' ticks.
     */
    uint64_t t = 0;
    uint64_t end = static_cast<uint64_t>(ms) * cpuFrequency * 1000;

    uint64_t last_tsc = rdtsc();
    while (t < end) {
        __asm __volatile("pause");
        uint64_t current_tsc = rdtsc();
        if (current_tsc < last_tsc) {
            t += (0xffffffffffffffff - last_tsc) + current_tsc + 1;
        } else {
            t += current_tsc - last_tsc;
        }
        last_tsc = current_tsc;
    }
}
