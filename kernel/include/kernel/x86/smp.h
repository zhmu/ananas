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

class Result;

namespace smp {
void Init();
void Prepare();
void PanicOthers();
void BroadcastSchedule();
} // namespace smp
#endif

#endif /* __X86_SMP_H__ */
