#ifndef __X86_SMAP_H__
#define __X86_SMAP_H__

#define SMAP_TYPE_MEMORY   0x01
#define SMAP_TYPE_RESERVED 0x02
#define SMAP_TYPE_RECLAIM  0x03
#define SMAP_TYPE_NVS      0x04

struct SMAP_ENTRY {
	uint32_t base_lo, base_hi;
	uint32_t len_lo, len_hi;
	uint32_t type;
} __attribute__((packed));

#endif /* __X86_SMAP_H__ */
