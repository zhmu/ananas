#include <machine/frame.h>
#include <machine/pcpu.h>
#include <machine/smp.h>
#include <ananas/asmsymbols.h>
#include <ananas/pcpu.h>

ASM_SYMBOL(SF_TRAPNO, offsetof(struct STACKFRAME, sf_trapno));
ASM_SYMBOL(SF_EAX,    offsetof(struct STACKFRAME, sf_eax));
ASM_SYMBOL(SF_EBX,    offsetof(struct STACKFRAME, sf_ebx));
ASM_SYMBOL(SF_ECX,    offsetof(struct STACKFRAME, sf_ecx));
ASM_SYMBOL(SF_EDX,    offsetof(struct STACKFRAME, sf_edx));
ASM_SYMBOL(SF_EBP,    offsetof(struct STACKFRAME, sf_ebp));
ASM_SYMBOL(SF_ESP,    offsetof(struct STACKFRAME, sf_esp));
ASM_SYMBOL(SF_EDI,    offsetof(struct STACKFRAME, sf_edi));
ASM_SYMBOL(SF_ESI,    offsetof(struct STACKFRAME, sf_esi));
ASM_SYMBOL(SF_DS,     offsetof(struct STACKFRAME, sf_ds));
ASM_SYMBOL(SF_ES,     offsetof(struct STACKFRAME, sf_es));
ASM_SYMBOL(SF_FS,     offsetof(struct STACKFRAME, sf_fs));
ASM_SYMBOL(SF_GS,     offsetof(struct STACKFRAME, sf_gs));
ASM_SYMBOL(SF_ERRNUM, offsetof(struct STACKFRAME, sf_errnum));
ASM_SYMBOL(SF_EIP,    offsetof(struct STACKFRAME, sf_eip));
ASM_SYMBOL(SF_CS,     offsetof(struct STACKFRAME, sf_cs));
ASM_SYMBOL(SF_EFLAGS, offsetof(struct STACKFRAME, sf_eflags));

ASM_SYMBOL(T_ARG1,    offsetof(struct THREAD, md_arg1));
ASM_SYMBOL(T_ARG2,    offsetof(struct THREAD, md_arg2));
ASM_SYMBOL(T_ESP0,    offsetof(struct THREAD, md_esp0));

ASM_SYMBOL(PCPU_CURTHREAD, offsetof(struct PCPU, curthread));
ASM_SYMBOL(PCPU_FPUCTX, offsetof(struct PCPU, fpu_context));
ASM_SYMBOL(PCPU_NESTEDIRQ, offsetof(struct PCPU, nested_irq));

ASM_SYMBOL(SMP_CPU_OFFSET,	offsetof(struct IA32_SMP_CONFIG, cfg_cpu));
ASM_SYMBOL(SMP_NUM_CPUS,	offsetof(struct IA32_SMP_CONFIG, cfg_num_cpus));
ASM_SYMBOL(SMP_CPU_SIZE,	sizeof(struct IA32_CPU));
ASM_SYMBOL(SMP_CPU_LAPICID,	offsetof(struct IA32_CPU, lapic_id));
ASM_SYMBOL(SMP_CPU_STACK,	offsetof(struct IA32_CPU, stack));
ASM_SYMBOL(SMP_CPU_GDT,		offsetof(struct IA32_CPU, gdt));
