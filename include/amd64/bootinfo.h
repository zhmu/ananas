#ifndef __AMD64_BOOTINFO_H__
#define __AMD64_BOOTINFO_H__

struct BOOTINFO {
	uint32_t	bi_ident;
#define BI_IDENT	0xBADF00D2
	uint32_t	bi_e820_addr;

};

#endif /* __AMD64_BOOTINFO_H__ */
