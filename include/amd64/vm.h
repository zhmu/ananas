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

/* GDT entry index numbers and corresponding selectors */
#define GDT_IDX_KERNEL_CODE	1
#define GDT_SEL_KERNEL_CODE	(GDT_IDX_KERNEL_CODE * 16)
#define GDT_IDX_KERNEL_DATA	2
#define GDT_SEL_KERNEL_DATA	(GDT_IDX_KERNEL_DATA * 16)
#define GDT_IDX_USER_CODE	3
#define GDT_SEL_USER_CODE	(GDT_IDX_USER_CODE * 16)
#define GDT_IDX_USER_DATA	4
#define GDT_SEL_USER_DATA	(GDT_IDX_USER_DATA * 16)
#define GDT_IDX_TASK		5
#define GDT_SEL_TASK		(GDT_IDX_TASK * 16)

#endif /* __AMD64_VM_H__ */
