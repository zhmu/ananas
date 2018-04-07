#include <ananas/types.h>
#include "kernel/x86/smp.h"
#include "kernel/asmsymbols.h"
#include "kernel/pcpu.h"
#include "kernel/thread.h"
#include "kernel-md/frame.h"
#include "kernel-md/pcpu.h"
#include "kernel-md/thread.h"
#include "../sys/syscall.h"

ASM_SYMBOL(SF_TRAPNO, offsetof(struct STACKFRAME, sf_trapno));
ASM_SYMBOL(SF_RAX,    offsetof(struct STACKFRAME, sf_rax));
ASM_SYMBOL(SF_RBX,    offsetof(struct STACKFRAME, sf_rbx));
ASM_SYMBOL(SF_RCX,    offsetof(struct STACKFRAME, sf_rcx));
ASM_SYMBOL(SF_RDX,    offsetof(struct STACKFRAME, sf_rdx));
ASM_SYMBOL(SF_RBP,    offsetof(struct STACKFRAME, sf_rbp));
ASM_SYMBOL(SF_RSI,    offsetof(struct STACKFRAME, sf_rsi));
ASM_SYMBOL(SF_RDI,    offsetof(struct STACKFRAME, sf_rdi));
ASM_SYMBOL(SF_RSP,    offsetof(struct STACKFRAME, sf_rsp));
ASM_SYMBOL(SF_R8,     offsetof(struct STACKFRAME, sf_r8 ));
ASM_SYMBOL(SF_R9,     offsetof(struct STACKFRAME, sf_r9 ));
ASM_SYMBOL(SF_R10,    offsetof(struct STACKFRAME, sf_r10));
ASM_SYMBOL(SF_R11,    offsetof(struct STACKFRAME, sf_r11));
ASM_SYMBOL(SF_R12,    offsetof(struct STACKFRAME, sf_r12));
ASM_SYMBOL(SF_R13,    offsetof(struct STACKFRAME, sf_r13));
ASM_SYMBOL(SF_R14,    offsetof(struct STACKFRAME, sf_r14));
ASM_SYMBOL(SF_R15,    offsetof(struct STACKFRAME, sf_r15));
ASM_SYMBOL(SF_ERRNUM, offsetof(struct STACKFRAME, sf_errnum));
ASM_SYMBOL(SF_RFLAGS, offsetof(struct STACKFRAME, sf_rflags));
ASM_SYMBOL(SF_RIP,    offsetof(struct STACKFRAME, sf_rip));
ASM_SYMBOL(SF_CS,     offsetof(struct STACKFRAME, sf_cs));
ASM_SYMBOL(SF_DS,     offsetof(struct STACKFRAME, sf_ds));
ASM_SYMBOL(SF_ES,     offsetof(struct STACKFRAME, sf_es));
ASM_SYMBOL(SF_SS,     offsetof(struct STACKFRAME, sf_ss));
ASM_SYMBOL(SF_SIZE,   sizeof(struct STACKFRAME));

ASM_SYMBOL(T_FRAME,   offsetof(Thread, t_frame));
ASM_SYMBOL(T_FLAGS,   offsetof(Thread, t_flags));
ASM_SYMBOL(T_MDFLAGS, offsetof(Thread, t_md_flags));

ASM_SYMBOL(PCPU_CURTHREAD, offsetof(struct PCPU, curthread));
ASM_SYMBOL(PCPU_NESTEDIRQ, offsetof(struct PCPU, nested_irq));
ASM_SYMBOL(PCPU_SYSCALLRSP, offsetof(struct PCPU, syscall_rsp));
ASM_SYMBOL(PCPU_RSP0, offsetof(struct PCPU, rsp0));

ASM_SYMBOL(SYSARG_NUM,	offsetof(struct SYSCALL_ARGS, number));
ASM_SYMBOL(SYSARG_ARG1,	offsetof(struct SYSCALL_ARGS, arg1));
ASM_SYMBOL(SYSARG_ARG2,	offsetof(struct SYSCALL_ARGS, arg2));
ASM_SYMBOL(SYSARG_ARG3,	offsetof(struct SYSCALL_ARGS, arg3));
ASM_SYMBOL(SYSARG_ARG4,	offsetof(struct SYSCALL_ARGS, arg4));
ASM_SYMBOL(SYSARG_ARG5,	offsetof(struct SYSCALL_ARGS, arg5));
ASM_SYMBOL(SYSARG_SIZE,	sizeof(struct SYSCALL_ARGS));

ASM_SYMBOL(SMP_CPU_OFFSET,	offsetof(struct X86_SMP_CONFIG, cfg_cpu));
ASM_SYMBOL(SMP_NUM_CPUS,	offsetof(struct X86_SMP_CONFIG, cfg_num_cpus));
ASM_SYMBOL(SMP_CPU_SIZE,	sizeof(struct X86_CPU));
ASM_SYMBOL(SMP_CPU_LAPICID,	offsetof(struct X86_CPU, lapic_id));
ASM_SYMBOL(SMP_CPU_PCPU,	offsetof(struct X86_CPU, pcpu));
ASM_SYMBOL(SMP_CPU_STACK,	offsetof(struct X86_CPU, stack));
