/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#define MAX_IRQS 256

namespace md
{
    namespace interrupts
    {
        static inline void Enable() { __asm __volatile("sti"); }

        static inline void Disable() { __asm __volatile("cli"); }

        static inline void Restore(int enabled)
        {
            __asm __volatile(
                // get flags in %rdx
                "pushfq\n"
                "popq %%rdx\n"
                // mask interrupt flag and re-enable flag if needed
                "andq $~0x200, %%rdx\n"
                "orl %0, %%edx\n"
                // activate new flags
                "pushq %%rdx\n"
                "popfq\n"
                :
                : "a"(enabled)
                : "%rdx");
        }

        static inline int Save()
        {
            int r;
            __asm __volatile(
                // get flags
                "pushfq\n"
                "popq %%rdx\n"
                // but only the interrupt flag
                "andq $0x200, %%rdx\n"
                "movl %%edx, %0"
                : "=r"(r)
                :
                : "%rdx");
            return r;
        }

        static inline int SaveAndDisable()
        {
            int status = Save();
            Disable();
            return status;
        }

        static inline void Relax() { __asm __volatile("hlt"); }

    } // namespace interrupts
} // namespace md
