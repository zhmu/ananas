#ifndef __X86_SMP_H__
#define __X86_SMP_H__

#define SMP_IPI_FIRST		0xf0
#define SMP_IPI_COUNT		4
#define SMP_IPI_PANIC		0xf0	/* IPI used to trigger panic situation on other CPU's */
#define SMP_IPI_SCHEDULE	0xf2	/* IPI used to trigger re-schedule */

#ifndef ASM
struct X86_CPU {
	int		cpu_lapic_id = -1;	/* Local APIC ID */
	char*		cpu_stack = nullptr;	/* CPU stack */
	void*		cpu_pcpu = nullptr;	/* PCPU pointer */
	char*		cpu_gdt = nullptr;	/* Global Descriptor Table */
	char*		cpu_tss = nullptr;	/* Task State Segment */
};

struct X86_INTERRUPT {
	int                 int_source_no;
	int                 int_dest_no;
	int                 int_polarity;
#define INTERRUPT_POLARITY_LOW	1
#define INTERRUPT_POLARITY_HIGH	2
	int                 int_trigger;
#define INTERRUPT_TRIGGER_EDGE	1
#define INTERRUPT_TRIGGER_LEVEL	2
	struct X86_IOAPIC* int_ioapic;
};

class Result;

void smp_init();

void smp_prepare();
void smp_prepare_config(struct X86_SMP_CONFIG* cfg);
void smp_panic_others();
void smp_broadcast_schedule();
#endif

#endif /* __X86_SMP_H__ */
