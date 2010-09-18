#ifndef __AMD64_BOOTINFO_H__
#define __AMD64_BOOTINFO_H__

struct BOOTINFO {
	uint32_t	bi_ident;		/* Magic number */
#define BI_IDENT	0xBADF00D2
	uint32_t	bi_e820_addr;		/* Pointer to E820 memory map */
	uint64_t	bi_kern_end;		/* Kernel end address */
};

#endif /* __AMD64_BOOTINFO_H__ */
