#ifndef __I386_SMP_H__
#define __I386_SMP_H__

#define SMP_IPI_FIRST		0xf0
#define SMP_IPI_COUNT		4
#define SMP_IPI_PANIC		0xf0	/* IPI used to trigger panic situation on other CPU's */
#define SMP_IPI_SCHEDULE	0xf2	/* IPI used to trigger re-schedule */

#ifndef ASM
struct IA32_CPU {
	/* NOTE: order is important - refer to mp_stub.S */
	int		lapic_id;	/* Local APIC ID */
	char*		stack;		/* CPU stack */
	char*		gdt;		/* Global Descriptor Table */
	char*		tss;		/* Task State Segment */
};

struct IA32_BUS {
	int		id;
	int		type;
#define BUS_TYPE_UNKNOWN	0
#define BUS_TYPE_ISA		1
};

struct IA32_INTERRUPT {
	int                 source_no;
	int                 dest_no;
	struct IA32_IOAPIC* ioapic;
	struct IA32_BUS*    bus;
};

struct IA32_SMP_CONFIG {
	int cfg_num_ioapics;
	int cfg_num_cpus;
	int cfg_num_ints;
	int cfg_num_busses;
	struct IA32_CPU* cfg_cpu;
	struct IA32_IOAPIC* cfg_ioapic;
	struct IA32_INTERRUPT* cfg_int;
	struct IA32_BUS* cfg_bus;
};

errorcode_t smp_init();
uint32_t get_num_cpus();

void smp_prepare_config(struct IA32_SMP_CONFIG* cfg);
void smp_panic_others();
void smp_broadcast_schedule();
#endif

#endif /* __I386_SMP_H__ */
