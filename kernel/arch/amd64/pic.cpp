/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/irq.h"
#include "kernel-md/io.h"
#include "kernel-md/pic.h"

void x86_pic_mask_all()
{
    /*
     * Reset the PIC by re-initalizating it; this also remaps all interrupts to
     * vectors 0x20-0x2f. Note that we don't actually _want_ the PIC to actually
     * service interrupts, but this clears the IRR register - which seems the
     * only way to do it.
     */
    constexpr auto io_wait = []() {
        for (int i = 0; i < 10; i++)
            /* nothing */;
    };

    // Start initialization: the PIC will wait for 3 command bytes
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    // Data byte 1 is the interrupt vector offset - program for interrupt 0x20-0x2f
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();
    // Data byte 2 tells the PIC how they are wired
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();
    // Data byte 3 contains environment flags
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    // Finally, disable all interrupts by masking them
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}
