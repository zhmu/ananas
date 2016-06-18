#ifndef __I386_VM_H__
#define __I386_VM_H__

#include <sys/types.h>

/*
 * Overview
 * --------
 *
 * For Ananas/i386, we use the following virtual memory map; the physical
 * memory entries are irrelevant.
 *
 * 0x0000 0000         +---------------------------+
 *                     | Unused                    |
 *                     /                           /
 *                     |                           |
 * 0x0100 0000 (1MB)   +---------------------------+
 *                     | Process memory            |
 *                     /                           /
 *                     |                           |
 * 0xc000 0000 (3GB)   +---------------------------+
 *                     | Kernel memory             |
 *                     /                           /
 *                     |                           |
 * 0xffff ffff (4GB)   +---------------------------+
 * 
 *
 * This gives us ~1GB of kernel virtual memory and ~3GB of process memory.
 *
 * On i386, there is a 4KB page-directory (PD) which contains pointers to 4KB
 * page-tables (PT). Each page-table contains 4-byte entries (PTE's) which
 * refer to the actual memory.
 *
 * We statically allocate the kernel PD and PT's during boot (they are placed
 * after the kernel itself). The reason is that the kernel is capable of
 * addressing up to 1GB of memory, and it needs to be able to map this memory
 * in order to use it.
 *
 * As every PT is 4KB and every PTE is 4 bytes, a single PT can contain up to
 * 4096 / 4 = 1024 entries, which means a single PT can address 1024 * 4096 =
 * 4MB of memory. This means we need a total of 1GB / 4MB = 256 PT's for the
 * kernel. User PT's will be allocated on the fly as necessary, there's no
 * need to statically place them somewhere.
 *
 * Kernel mappings 
 * ---------------
 *
 * As outlined above, there is an upper bound on the amount of kernel memory,
 * so we can allocate the kernel page tables statically during startup. We
 * will generally attempt to honor KVTOP(PTOKV(x)) = x; that is, we'll try
 * to map physical addresses directly to the kernel-space - this is
 * convenient for most purposes because the kernel doesn't really need much
 * more than ~1GB memory and it makes bookkeeping easy.
 *
 * However, there are times when we must map physical addresses above 1GB;
 * for example, due to data structures there and the like. This needs
 * special care, because simply OR-ing KERNBASE to them isn't enough as
 * they have such bits already set. This means we have to reserve some
 * specific virtual addresses to where we can map such physical addresses.
 * 
 * Addresses from KMEM_DYNAMIC_VA_START to KMEM_DYNAMIC_VA_END are used for
 * this purpose; this gives a hard limit to the amount of things we can map,
 * but then again, total kernel memory usage can never exceed 1GB anyway so
 * this should be sufficient.
 */

/* First kernel PT; this is 3GB / 4MB = 768 */
#define VM_FIRST_KERNEL_PT	768

/* Number of kernel PT's (1GB / (1024 * 4KB) = 256, see above) */
#define VM_NUM_KERNEL_PTS	256

/* Amount of bits to shift away to get the corresponding PT */
#define VM_SHIFT_PT		22	/* log_2(4MB) */

/* Amount of bits to shift away to get the corresponding PTE */
#define VM_SHIFT_PTE		12	/* log_2(4KB) */

/* Convert a physical to a kernel virtual address */
#define PTOKV(x)		((x) | KERNBASE)

/* Convert a kernel virtual address to a physical address */
#define KVTOP(x)		((x) & ~KERNBASE)

/* Convert a physical address to the direct-mapped virtual address */
#define PA_TO_DIRECT_VA(x)		((x) | KERNBASE)

/* Base kernel virtual address from which we'll make mappings */
#define KMEM_DYNAMIC_VA_START 0xffd00000

/* Base kernel virtual address up to which mappings are made */
#define KMEM_DYNAMIC_VA_END 0xfff00000

/* Direct mapped KVA start; we can use use KVTOP()/PTOKV() for it */
#define KMEM_DIRECT_PA_START	0
#define KMEM_DIRECT_PA_END		(KMEM_DYNAMIC_VA_START - KERNBASE)

/* CR0 register bits */
#define CR0_PE		(1 << 0)	/* Protection Enable */
#define CR0_MP		(1 << 1)	/* Monitor Coprocessor */
#define CR0_EM		(1 << 2)	/* Emulation */
#define CR0_TS		(1 << 3)	/* Task Switched */
#define CR0_ET		(1 << 4)	/* Extension Type */
#define CR0_NE		(1 << 5)	/* Numerical Error */
#define CR0_WP		(1 << 16)	/* Write Protect */
#define CR0_AM		(1 << 18)	/* Alignment mask */
#define CR0_NW		(1 << 29)	/* Not Write-through */
#define CR0_CD		(1 << 30)	/* Cache Disable */
#define CR0_PG		(1 << 31)	/* Paging Enable */

/* Page Directory Entry bits */
#define PDE_P		(1 << 0)	/* Present */
#define PDE_RW		(1 << 1)	/* Read/Write */
#define PDE_US		(1 << 2)	/* User/Supervisor */
#define PDE_PWT		(1 << 3)	/* Write-Through */
#define PDE_PCD		(1 << 4)	/* Cache Disabled */
#define PDE_A		(1 << 5)	/* Accessed */
#define _PDE_R		(1 << 6)	/* Reserved */
#define PDE_PS		(1 << 7)	/* Page size */
#define PDE_G		(1 << 8)	/* Global page */

/* Page Table Entry bits */
#define PTE_P		(1 << 0)	/* Present */
#define PTE_RW		(1 << 1)	/* Read/Write */
#define PTE_US		(1 << 2)	/* User/Supervisor */
#define PTE_PWT		(1 << 3)	/* Write-Through */
#define PTE_PCD		(1 << 4)	/* Cache Disabled */
#define PTE_A		(1 << 5)	/* Accessed */
#define PTE_D		(1 << 6)	/* Dirty */
#define PTE_PAT		(1 << 7)	/* Page Table Attribute index */
#define PTE_G		(1 << 8)	/* Global page */

/* Segment Register bits */
#define SEG_S		(1 << 4)	/* S (Descriptor type) flag */
#define SEG_P		(1 << 7)	/* P (segment-present) flag */
#define SEG_AVL		(1 << 4)	/* Available for system software flag */
#define SEG_DB		(1 << 6)	/* default size flag */
#define SEG_G		(1 << 7)	/* G (granularity) flag */

#define SEG_TSKGATE_TYPE 5		/* Task gate */
#define SEG_IGATE_TYPE 6		/* Interrupt gate type (disables interrupts) */
#define SEG_TGATE_TYPE 7		/* Trap gate type (keeps interrupts intact) */
#define SEG_GATE_D	(1 << 3)	/* Size of gate */
#define SEG_GATE_P	(1 << 7)	/* Present */

/* Segment Register types */
#define SEG_TYPE_CODE	10 		/* GDT entry type: code, execute-read */
#define SEG_TYPE_DATA	2		/* GDT entry type: data, read/write */

/* Segment Register privilege levels */
#define SEG_DPL_SUPERVISOR	0	/* Descriptor Privilege Level (kernel) */
#define SEG_DPL_USER		3	/* Descriptor Privilege Level (user) */

/* GDT entry index numbers and corresponding selectors */
#define GDT_IDX_KERNEL_CODE	1
#define GDT_SEL_KERNEL_CODE	(GDT_IDX_KERNEL_CODE * 8)
#define GDT_IDX_KERNEL_DATA	2
#define GDT_SEL_KERNEL_DATA	(GDT_IDX_KERNEL_DATA * 8)
#define GDT_IDX_KERNEL_PCPU	3
#define GDT_SEL_KERNEL_PCPU	(GDT_IDX_KERNEL_PCPU * 8)
#define GDT_IDX_USER_CODE	4
#define GDT_SEL_USER_CODE	(GDT_IDX_USER_CODE * 8)
#define GDT_IDX_USER_DATA	5
#define GDT_SEL_USER_DATA	(GDT_IDX_USER_DATA * 8)
#define GDT_IDX_KERNEL_TASK	6
#define GDT_SEL_KERNEL_TASK	(GDT_IDX_KERNEL_TASK * 8)
#define GDT_IDX_FAULT_TASK	7
#define GDT_SEL_FAULT_TASK	(GDT_IDX_FAULT_TASK * 8)

#ifndef ASM

typedef struct VM_SPACE vmspace_t;

/* Maps relevant kernel addresses for a given thread */
void md_map_kernel(vmspace_t* vs);

/* Maps 'num_pages' at physical address 'phys' to virtual address 'virt' for vmspace 'vs' with flags 'flags' */
void md_map_pages(vmspace_t* vs, addr_t virt, addr_t phys, size_t num_pages, int flags);

/* Unmaps 'num_pages' at virtual address virt for vmspace 'vs' */
void md_unmap_pages(vmspace_t* vs, addr_t virt, size_t num_pages);

#endif

#endif /* __I386_VM_H__ */
