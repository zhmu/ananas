#include <ananas/types.h>
#include "kernel-md/frame.h"

size_t gdb_md_get_register_size(int regnum) { return (regnum < 17) ? 8 : 4; }

void* gdb_md_get_register(struct STACKFRAME* sf, int regnum)
{
    switch (regnum) {
        case 0:
            return &sf->sf_rax;
        case 1:
            return &sf->sf_rbx;
        case 2:
            return &sf->sf_rcx;
        case 3:
            return &sf->sf_rdx;
        case 4:
            return &sf->sf_rsi;
        case 5:
            return &sf->sf_rdi;
        case 6:
            return &sf->sf_rbp;
        case 7:
            return &sf->sf_rsp;
        case 8:
            return &sf->sf_r8;
        case 9:
            return &sf->sf_r9;
        case 10:
            return &sf->sf_r10;
        case 11:
            return &sf->sf_r11;
        case 12:
            return &sf->sf_r12;
        case 13:
            return &sf->sf_r13;
        case 14:
            return &sf->sf_r14;
        case 15:
            return &sf->sf_r15;
        case 16:
            return &sf->sf_rip;
        case 17:
            return &sf->sf_rflags;
        case 18:
            return &sf->sf_cs;
        case 19:
            return &sf->sf_ss;
    }
    return NULL;
}

int gdb_md_map_signal(struct STACKFRAME* sf)
{
    switch (sf->sf_trapno) {
        case 1: /* debug exception */
        case 3: /* breakpoint */
            return 5;
        case 4: /* into instruction (overflow) */
        case 5: /* bound instruction */
            return 16;
        case 6: /* Invalid opcode */
            return 4;
        case 0: /* divide by zero */
        case 7: /* coprocessor not available */
            return 8;
        case 9:  /* coprocessor segment overrun */
        case 10: /* Invalid TSS */
        case 11: /* Segment not present */
        case 12: /* stack exception */
        case 13: /* general protection */
        case 14: /* page fault */
            return 11;
        case 8:  /* double fault */
        case 16: /* coprocessor error */
        default: /* "software generated" */
            return 7;
    }
}

/* vim:set ts=2 sw=2: */
