#include <sys/types.h>

#ifndef __POWERPC_VM_H__
#define __POWERPC_VM_H__

#ifndef ASM
struct PTE {
#define PT_HI_V		0x80000000		/* Entry valid (=1) */
#define PT_HI_H		0x00000040		/* Hash function identifier */
	uint32_t pt_hi;
#define PT_LO_R		0x00000100		/* Reference bit */
#define PT_LO_C		0x00000080		/* Change bit */
	uint32_t pt_lo;
};

struct PTEG {
	struct PTE pte[8];
};

struct BAT {
	uint32_t bat_u;
	uint32_t bat_l;
};
#endif

/* BAT mapping length */
#define BAT_BL_128KB	0x0000
#define BAT_BL_256KB	0x0004
#define BAT_BL_512KB	0x000c
#define BAT_BL_1MB	0x001c
#define BAT_BL_2MB	0x003c
#define BAT_BL_4MB	0x007c
#define BAT_BL_8MB	0x00fc
#define BAT_BL_16MB	0x01fc
#define BAT_BL_32MB	0x03fc
#define BAT_BL_64MB	0x07fc
#define BAT_BL_128MB	0x0ffc
#define BAT_BL_256MB	0x1ffc

#define BAT_Vs		0x0002		/* Supervisor mode valid bit */
#define BAT_Vp		0x0001		/* User mode valid bit */

#define BAT_W		0x0040		/* Write-through */
#define BAT_I		0x0020		/* Caching inhebited */
#define BAT_M		0x0010		/* Memory coherence */
#define BAT_G		0x0008		/* Guarded storage */

#define BAT_PP_NONE	0x00		/* No access */
#define BAT_PP_RO	0x01		/* Read/only */
#define BAT_PP_RW	0x02		/* Read/write */

#define BAT_U(va,bl,v) \
	((va) | (bl) | (v))

#define BAT_L(pa,wimg,pp) \
	((pa) | (wimg) | (pp))

/* Segment register format */
#define SR_T		0x80000000	/* Format, must be zero */
#define SR_Ks		0x40000000	/* Supervisor state protection key */
#define SR_Kp		0x20000000	/* User-state protection key */
#define SR_N 		0x10000000	/* No-execute protection bit */

/* Machine Status Register flags */
#define MSR_POW		0x00040000	/* Power Management Enable */
#define MSR_ILE		0x00010000	/* Interrupt Little Endian Mode */
#define MSR_EE 		0x00008000	/* External Interrupt Enable */
#define MSR_PR 		0x00004000	/* Problem State */
#define MSR_FP 		0x00002000	/* Floating-Point Available */
#define MSR_ME 		0x00001000	/* Machine-Check Enable */
#define MSR_FE0		0x00000800	/* Floating-Point Exception Mode 0 */
#define MSR_SE		0x00000400	/* Single-Step Trace Enable */
#define MSR_BE		0x00000200	/* Branch Trace Enable */
#define MSR_FE1		0x00000100	/* Floating-Point Exception Mode 1 */
#define MSR_IP		0x00000040	/* Interrupt Prefix */
#define MSR_IR		0x00000020	/* Interrupt Relocate */
#define MSR_DR		0x00000010	/* Data Relocate */
#define MSR_RI		0x00000002	/* Recoverable Interrupt */
#define MSR_LE		0x00000001	/* Little Endian Mode */

/* Maximum number of VSID's that will be allocated */
#define MAX_VSIDS	8192
#define INVALID_VSID	0xfffff0

#ifndef ASM
void vm_map(addr_t addr, size_t num_pages);
void vm_mapto(addr_t virt, addr_t phys, size_t num_pages);
void vm_unmap(addr_t addr, size_t num_pages);
#endif

#endif /* __POWERPC_VM_H__ */
