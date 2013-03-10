#ifndef __ARM_VM_H__
#define __ARM_VM_H__

/*
 * First of all, ARM's suggestions are followed as regiously as possible:
 *
 * - since this port targets ARMv5+, subpages are enabled
 * - the S/R access permissions are deprecated and not used
 * - 1kB pages are deprecated and not used
 * - Fine page tables are deprecated and not used
 *
 * A page size of 4kB is used, which means only pages of that size need to be
 * mapped; this eliminates the need for both 1kB pages and fine page tables -
 * instead, everything will be mapped using small (4kB) pages in the coarse
 * page table layout. Below is a rough overview of the ARM's MMU and the
 * implications it has for us.
 *
 * The translation table (TT) is indexed using VA bits 20:31 (these are 12
 * bits, so this means there are 2**12 = 4096 entries). Every translation table
 * entry is 32 bits, so the entire TT consumes 4096 * 4 = 16kB of memory. Using
 * the translation table base register (TTBR) and VA bits 20:31, an entry is
 * looked up, which is called the first-level descriptor (B4.7.4)
 *
 * This second-level descriptor is a so-called 'coarse page table', and is
 * indexed using VA bits 12:19 (these are 8 bits, so this means there are 2**8
 * = 256 entries). Again, every entry is 32 bits, so the second level table
 * consumes 256 * 4 = 1kB of memory. Using the page table base address
 * contained first-level-descriptor's and VA bits 12:19, we end up with the
 * final piece, the second-level descriptor, which contains bits 12:31 of the
 * PA (bits 0:11 of the VA are copied as-is, resulting in a 4kB granulatity)
 *
 * Ananas/arm uses generally the same virtual memory mapping as Ananas/i386:
 * everything from KERNBASE to 0xffffffff is considered kernel memory - we shall
 * assume KERNBASE is 0xc000000 from here onwards.
 *
 * As VA bits 20:31 refer to the TT entry, this means we'll use 1024 entries
 * in total (3072 to 4096); every item refers to an 1kB second-level descriptor,
 * so we need a total of 1024 * 1024 = 1MB of memory for the kernel mappings.
 */

/* First kernel-memory TT mapping; it's in the TT so we need bits 20:31 */
#define VM_TT_KERNEL_START	(KERNBASE >> 20)

/* Number of kernel-specific entries in the TT; this is (0xFFFFFFFF - KERNBASE) >> 20 */
#define VM_TT_KERNEL_NUM	(4095 - VM_TT_KERNEL_START)

/* Size of the translation table, in bytes */
#define VM_TT_SIZE		16384

/* Translation table alignment - we need to keep bits 0:13 clear as outlined in B4-3 */
#define VM_TT_ALIGN		(1 << 14)

/* Size of a coarse translation table, in bytes */
#define VM_L2_TABLE_SIZE	1024

/* Access permission bits */
#define VM_AP_NONE		0				/* Kernel none User none */
#define VM_AP_KRW_UNONE		1				/* Kernel R/W  User none */
#define VM_AP_KRW_URO		2				/* Kernel R/W  User R/O  */
#define VM_AP_KRW_URW		3				/* Kernel R/W  User R/W  */

/* Translation table entry types */
#define VM_TT_TYPE_FAULT	0
#define VM_TT_TYPE_COARSE	1
#define VM_TT_TYPE_SECTION	2
#define VM_TT_TYPE_FINE		3

/* Coarse table entry types */
#define VM_COARSE_TYPE_FAULT	0
#define VM_COARSE_TYPE_LARGE	1
#define VM_COARSE_TYPE_SMALL	2
#define VM_COARSE_TYPE_SMALLEX	3

/* Coarse memory flags */
#define VM_COARSE_DEVICE	(1 << 2)			/* Shared device: uncachable, bufferable */

/* Macro used to replicate AP bits to entries 0..3 for small entries */
#define VM_COARSE_SMALL_AP(x)	(((x) << 4) | ((x) << 6) | ((x) << 8) | ((x) << 10))

/* CP15 register 1: Control Register (B4.9.2) */
#define CP15_CTRL_MMU		(1 << 0)			/* MMU enabled */
#define CP15_CTRL_ALIGNMENT	(1 << 1)			/* Alignment fault checking */
#define CP15_CTRL_WRITEBUFFER	(1 << 2)			/* Write buffer */
#define CP15_CTRL_SYSTEMPROT	(1 << 8)			/* System protection */
#define CP15_CTRL_ROMPROT	(1 << 9)			/* ROM protection */
#define CP15_CTRL_ICACHE	(1 << 12)			/* Instruction cache */

/* Convert a physical to a kernel virtual address */
#define PTOKV(x)		((x) | KERNBASE)

/* Convert a kernel virtual address to a physical address */
#define KVTOP(x)		((x) & ~KERNBASE)

#ifndef ASM
void vm_map(addr_t addr, size_t num_pages);
void vm_mapto(addr_t virt, addr_t phys, size_t num_pages);
void vm_unmap(addr_t addr, size_t num_pages);
#endif

#endif /* __ARM_VM_H__ */
