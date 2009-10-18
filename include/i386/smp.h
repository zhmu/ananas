#ifndef __I386_SMP_H__
#define __I386_SMP_H__

struct MP_FLOATING_POINTER {
	uint32_t	signature;
#define MP_FPS_SIGNATURE	0x5f504d5f	/* _MP_ */
	uint32_t	phys_ptr;
	uint8_t		length;
	uint8_t		spec_rev;
	uint8_t		checksum;
	uint8_t		feature1;
	uint8_t		feature2;
#define MP_FPS_FEAT2_IMCRP	0x80
	uint8_t		feature3;
	uint8_t		feature4;
	uint8_t		feature5;
} __attribute__((packed));

struct MP_CONFIGURATION_TABLE {
	uint32_t	signature;
#define MP_CT_SIGNATURE		0x504d4350	/* PCMP */
	uint16_t	base_len;
	uint8_t		spec_rev;
	uint8_t		checksum;
	uint8_t		oem_id[8];
	uint8_t		product_id[12];
	uint32_t	oem_ptr;
	uint16_t	oem_size;
	uint16_t	entry_count;
	uint32_t	lapic_addr;
	uint16_t	ext_length;
	uint8_t		ext_checksum;
	uint8_t		_reserved;
} __attribute__((packed));

struct MP_ENTRY_PROCESSOR {
	uint8_t		lapic_id;
	uint8_t		lapic_version;
	uint8_t		flags;
#define MP_PROC_FLAG_EN		0x01	/* Processor enabled */
#define MP_PROC_FLAG_BP		0x02	/* Bootstrap processor */
	uint32_t	signature;
	uint32_t	features;
	uint32_t	reserved[2];
} __attribute__((packed));

struct MP_ENTRY_BUS {
	uint8_t		bus_id;
	uint8_t		type[6];
} __attribute__((packed));

struct MP_ENTRY_IOAPIC {
	uint8_t		ioapic_id;
	uint8_t		version;
	uint8_t		flags;
	uint32_t	addr;
} __attribute__((packed));

struct MP_ENTRY_INTERRUPT {
	uint8_t		type;
	uint16_t	flags;
	uint8_t		source_busid;
	uint8_t		source_irq;
	uint8_t		dest_ioapicid;
	uint8_t		dest_ioapicint;
} __attribute__((packed));

struct MP_ENTRY {
	uint8_t		type;
#define MP_ENTRY_TYPE_PROCESSOR	0x00
#define MP_ENTRY_TYPE_BUS	0x01
#define MP_ENTRY_TYPE_IOAPIC	0x02
#define MP_ENTRY_TYPE_IOINT	0x03
#define MP_ENTRY_TYPE_LINT	0x04
	union {
		struct MP_ENTRY_PROCESSOR	proc;
		struct MP_ENTRY_BUS		bus;
		struct MP_ENTRY_IOAPIC		ioapic;
		struct MP_ENTRY_INTERRUPT	interrupt;
	} u;
} __attribute__((packed));

struct IA32_CPU {
	/* NOTE: order is important - refer to mp_stub.S */
	uint8_t		lapic_id;	/* Local APIC ID */
	char*		stack;		/* CPU stack */
	char*		gdt;		/* Global Descriptor Table */
	char*		tss;		/* Task State Segment */
};

uint32_t get_num_cpus();
struct IA32_CPU* get_cpu_struct(int i);


#endif /* __I386_SMP_H__ */
