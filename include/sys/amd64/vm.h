#include "types.h"

#ifndef __AMD64_VM_H__
#define __AMD64_VM_H__

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

/* Segment Register privilege levels */
#define SEG_DPL_SUPERVISOR	0	/* Descriptor Privilege Level (kernel) */
#define SEG_DPL_USER		3	/* Descriptor Privilege Level (user) */

/* Machine Specific Registers */
#define MSR_EFER		0xc0000080
 #define MSR_EFER_SCE		(1 <<  0)	/* System Call Extensions */
 #define MSR_EFER_LME		(1 <<  8)	/* Long Mode Enable */
 #define MSR_EFER_LMA		(1 << 10)	/* Long Mode Active */
 #define MSR_EFER_NXE		(1 << 11)	/* No-Execute Enable */
 #define MSR_EFER_SVME		(1 << 12)	/* Secuure VM Enable */
 #define MSR_EFER_FFXSR		(1 << 14)	/* Fast FXSAVE/FXRSTOR */
#define MSR_STAR		0xc0000081
#define MSR_LSTAR		0xc0000082
#define MSR_CSTAR		0xc0000083
#define MSR_SFMASK		0xc0000084
#define MSR_FS_BASE		0xc0000100
#define MSR_GS_BASE		0xc0000101
#define MSR_KERNEL_GS_BASE	0xc0000102

/* CR0 specific flags */
#define CR0_TS			(1 << 3)	/* Task switched */

/* CR4 specific flags */
#define CR4_OSFXSR		(1 << 9)	/* OS saves/restores SSE state */
#define CR4_OSXMMEXCPT		(1 << 10)	/* OS will handle SIMD exceptions */

/*
 * GDT entry selectors, which are the offset in the GDT. We don't use indexes
 * here because the task entry is 16 bytes whereas everything else is 8 bytes.
 */
#define GDT_SEL_KERNEL_CODE	0x08
#define GDT_SEL_KERNEL_DATA	0x10
#define GDT_SEL_USER_DATA	0x18
#define GDT_SEL_USER_CODE	0x20
#define GDT_SEL_TASK		0x28
#define GDT_LENGTH		(GDT_SEL_TASK + 0x10)

#ifndef ASM

void vm_mapto_pagedir(uint64_t* pml4, addr_t virt, addr_t phys, size_t num_pages, uint32_t user);
addr_t vm_get_phys(uint64_t* pagedir, addr_t addr, int write);
void vm_map_pagedir(uint64_t* pml4, addr_t addr, size_t num_pages, uint32_t user);
void vm_map(addr_t addr, size_t num_pages);
void vm_mapto(addr_t virt, addr_t phys, size_t num_pages);
void* vm_map_device(addr_t addr, size_t len);
void vm_map_kernel_addr(void* pml4);

#endif

#endif /* __AMD64_VM_H__ */
