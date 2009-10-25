#include "types.h"

#ifndef __TRAMPOLINE_H__
#define __TRAMPOLINE_H__

#define TRAMP_BASE	0x500000

/* Control Register flags */
#define CR0_PGE (1 << 31)
#define CR4_PAE	(1 << 5)

/* Extended Feature Register and flags */
#define MSR_EFER	0xc0000080
#define EFER_SCE	(1 << 0)
#define EFER_LME	(1 << 8)

/* Page entry flags */
#define PE_P		(1ULL <<  0)
#define PE_RW		(1ULL <<  1)
#define PE_US		(1ULL <<  2)
#define PE_PWT		(1ULL <<  3)
#define PE_PCD		(1ULL <<  4)
#define PE_A		(1ULL <<  5)
#define PE_D		(1ULL <<  6)
#define PE_PS		(1ULL <<  7)
#define PE_G		(1ULL <<  8)
#define PE_PAT		(1ULL << 12)
#define PE_NX		(1ULL << 63)

/* Rounds a page to a 2MB boundary */
#define ROUND_2MB(x) ((x) & 0xffe00000)

#ifndef ASM

/* init.c */
void vm_map(uint64_t dest, addr_t src, int num_pages);

/* lib.c */
void* memcpy(void* dst, const void* src, size_t len);
void* memset(void* dst, int c, size_t len);

/* relocate.c */
int relocate_kernel();

#endif

#endif /* __TRAMPOLINE_H__ */
