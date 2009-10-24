#include "types.h"

#ifndef __TRAMPOLINE_H__
#define __TRAMPOLINE_H__

#define CR0_PGE (1 << 31)
#define CR4_PAE	(1 << 5)

#define MSR_EFER	0xc0000080

#define EFER_SCE	(1 << 0)
#define EFER_LME	(1 << 8)

/* Rounds a page to a 2MB boundary */
#define ROUND_2MB(x) ((x) & 0xffe00000)

#ifndef ASM

/* init.c */
void vm_map(uint64_t dest, addr_t src, int num_pages);

/* lib.c */
void* memcpy(void* dst, const void* src, size_t len);
void* memset(void* dst, int c, size_t len);

#endif

#endif /* __TRAMPOLINE_H__ */
