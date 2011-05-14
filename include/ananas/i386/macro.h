#ifndef __I386_MACRO_H__
#define __I386_MACRO_H__

#define GDT_SET_ENTRY32(ptr, num, type, dpl, base, limit) do { \
	uint8_t* p = ((uint8_t*)ptr + num * 8); \
	/* Segment Limit 15:00 = 4GB */ \
	p[0] = (limit >> 8) & 0xff; p[1] = (limit) & 0xff; \
	/* Base Address 15:00 */ \
	p[2] = (base); p[3] = ((base) >> 8) & 0xff; \
	/* Base Address 23:15 */ \
	p[4] = (base >> 16) & 0xff; \
	/* Type 8:11, System 12, Privilege Level 13:14, Present 15 */ \
	p[5] = type | SEG_S | (dpl << 5) | SEG_P; \
	/* Seg. Limit 19:16, AVL 20, D/B 22, G 23 */ \
	p[6] = ((limit >> 16) & 0xf) | SEG_DB | SEG_G; \
	/* Base 31:24 */ \
	p[7] = (base >> 24) & 0xff; \
} while(0);

#define GDT_SET_ENTRY16(ptr, num, type, dpl, base) do { \
	uint8_t* p = ((uint8_t*)ptr + num * 8); \
	/* Segment Limit 15:00 = 64KB */ \
	p[0] = 0xff; p[1] = 0xff; \
	/* Base Address 15:00 */ \
	p[2] = (base) & 0xff; p[3] = ((base) >> 8) & 0xff; \
	/* Base Address 23:15 */ \
	p[4] = ((base) >> 16) & 0xff; \
	/* Type 8:11, System 12, Privilege Level 13:14, Present 15 */ \
	p[5] = type | SEG_S | (dpl << 5) | SEG_P; \
	/* Seg. Limit 19:16, AVL 20, D/B 22, G 23 */ \
	p[6] = 0; \
	/* Base 31:24 */ \
	p[7] = ((base) >> 24) & 0xff; \
} while(0);

#define GDT_SET_TSS(ptr, num, dpl, base, size) do { \
	uint8_t* p = ((uint8_t*)ptr + num * 8); \
	/* Segment Limit 15:00  */ \
	p[0] = (size) & 0xff; p[1] = ((size) >> 8) & 0xff; \
	/* Base Address 15:00 */ \
	p[2] = (base) & 0xff; p[3] = ((base) >> 8) & 0xff; \
	/* Base Address 23:15 */ \
	p[4] = (base >> 16) & 0xff; \
	/* Type 8:11, Zero 12, Privilege Level 13:14, Present 15 */ \
	p[5] = 0x9 | (dpl << 5) | SEG_P; \
	/* Seg. Limit 19:16, AVL 20, G 23 */ \
	p[6] = ((size) >> 16) & 0x7; \
	/*  Base 24:31 */ \
	p[7] = ((base) >> 24) & 0xff; \
} while (0);

/*
 * Convenience macro to create a XXXr 6-byte 'limit, base' variable
 * which are needed for LGDT and LIDT.
 */
#define MAKE_RREGISTER(name, x, num_entries) \
	uint8_t name[6]; \
	do { \
		name[0] = ((num_entries * 8) - 1) & 0xff; \
		name[1] = ((num_entries * 8) - 1) >> 8; \
		name[2] = ((addr_t)(x)      ) & 0xff; \
		name[3] = ((addr_t)(x) >>  8) & 0xff; \
		name[4] = ((addr_t)(x) >> 16) & 0xff; \
		name[5] = ((addr_t)(x) >> 24) & 0xff; \
	} while (0);

#define IDT_SET_ENTRY(num, type, dpl, handler) do { \
	extern void* handler; \
	IDT_SET_ENTRY_DIRECT(num, type, dpl, &handler); \
} while(0);

#define IDT_SET_ENTRY_DIRECT(num, type, dpl, addr) do { \
	uint8_t* p = ((uint8_t*)&idt + num * 8); \
	/* Offset 15:0 */ \
	p[0] = (((addr_t)addr)     ) & 0xff; \
	p[1] = (((addr_t)addr) >> 8) & 0xff; \
	/* Segment Selector */ \
	p[2] = ((GDT_IDX_KERNEL_CODE * 8)     ) & 0xff; \
	p[3] = ((GDT_IDX_KERNEL_CODE * 8) >> 8) & 0xff; \
	/* Reserved */ \
	p[4] = 0x00; \
	/* Type 9:10, size of gate 11, DPL 13:14, Present 15 */ \
	p[5] = type | SEG_GATE_D | (dpl << 5) | SEG_GATE_P; \
	/* Offset 31:16 */ \
	p[6] = (((addr_t)addr) >> 16) & 0xff; \
	p[7] = (((addr_t)addr) >> 24) & 0xff; \
} while(0);

#define GET_EFLAGS() ({ \
		uint32_t eflags; \
		__asm __volatile("pushfl; popl %0" : "=r" (eflags)); \
		eflags; \
	})

#endif /* __I386_MACRO_H__ */
