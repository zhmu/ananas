#ifndef __X86_SMP_H__
#define __X86_SMP_H__

#define SMP_IPI_FIRST		0xf0
#define SMP_IPI_COUNT		4
#define SMP_IPI_PANIC		0xf0	/* IPI used to trigger panic situation on other CPU's */
#define SMP_IPI_SCHEDULE	0xf2	/* IPI used to trigger re-schedule */

#ifndef ASM
struct X86_CPU {
	/* NOTE: order is important - refer to mp_stub.S */
	int		lapic_id;	/* Local APIC ID */
	char*		stack;		/* CPU stack */
#ifdef __amd64__
	void*		pcpu;		/* PCPU pointer */
#endif
	char*		gdt;		/* Global Descriptor Table */
	char*		tss;		/* Task State Segment */
};

struct X86_BUS {
	int		id;
	int		type;
#define BUS_TYPE_UNKNOWN	0
#define BUS_TYPE_ISA		1
};

struct X86_INTERRUPT {
	int                 source_no;
	int                 dest_no;
	struct X86_IOAPIC* ioapic;
	struct X86_BUS*    bus;
};

struct X86_SMP_CONFIG {
	int cfg_num_ioapics;
	int cfg_num_cpus;
	int cfg_num_ints;
	int cfg_num_busses;
	struct X86_CPU* cfg_cpu;
	struct X86_IOAPIC* cfg_ioapic;
	struct X86_INTERRUPT* cfg_int;
	struct X86_BUS* cfg_bus;
};

class Result;

Result smp_init();
uint32_t get_num_cpus();

void smp_prepare();
void smp_prepare_config(struct X86_SMP_CONFIG* cfg);
void smp_panic_others();
void smp_broadcast_schedule();
#endif

#endif /* __X86_SMP_H__ */
