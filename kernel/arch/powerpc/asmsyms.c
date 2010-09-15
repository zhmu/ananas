#include <sys/pcpu.h>
#include <sys/syscall.h>
#include <sys/asmsymbols.h>

ASM_SYMBOL(SF_R0,    offsetof(struct STACKFRAME, sf_reg[ 0]));
ASM_SYMBOL(SF_R1,    offsetof(struct STACKFRAME, sf_reg[ 1]));
ASM_SYMBOL(SF_R2,    offsetof(struct STACKFRAME, sf_reg[ 2]));
ASM_SYMBOL(SF_R3,    offsetof(struct STACKFRAME, sf_reg[ 3]));
ASM_SYMBOL(SF_R4,    offsetof(struct STACKFRAME, sf_reg[ 4]));
ASM_SYMBOL(SF_R5,    offsetof(struct STACKFRAME, sf_reg[ 5]));
ASM_SYMBOL(SF_R6,    offsetof(struct STACKFRAME, sf_reg[ 6]));
ASM_SYMBOL(SF_R7,    offsetof(struct STACKFRAME, sf_reg[ 7]));
ASM_SYMBOL(SF_R8,    offsetof(struct STACKFRAME, sf_reg[ 8]));
ASM_SYMBOL(SF_R9,    offsetof(struct STACKFRAME, sf_reg[ 9]));
ASM_SYMBOL(SF_R10,   offsetof(struct STACKFRAME, sf_reg[10]));
ASM_SYMBOL(SF_R11,   offsetof(struct STACKFRAME, sf_reg[11]));
ASM_SYMBOL(SF_R12,   offsetof(struct STACKFRAME, sf_reg[12]));
ASM_SYMBOL(SF_R13,   offsetof(struct STACKFRAME, sf_reg[13]));
ASM_SYMBOL(SF_R14,   offsetof(struct STACKFRAME, sf_reg[14]));
ASM_SYMBOL(SF_R15,   offsetof(struct STACKFRAME, sf_reg[15]));
ASM_SYMBOL(SF_R16,   offsetof(struct STACKFRAME, sf_reg[16]));
ASM_SYMBOL(SF_R17,   offsetof(struct STACKFRAME, sf_reg[17]));
ASM_SYMBOL(SF_R18,   offsetof(struct STACKFRAME, sf_reg[18]));
ASM_SYMBOL(SF_R19,   offsetof(struct STACKFRAME, sf_reg[19]));
ASM_SYMBOL(SF_R20,   offsetof(struct STACKFRAME, sf_reg[20]));
ASM_SYMBOL(SF_R21,   offsetof(struct STACKFRAME, sf_reg[21]));
ASM_SYMBOL(SF_R22,   offsetof(struct STACKFRAME, sf_reg[22]));
ASM_SYMBOL(SF_R23,   offsetof(struct STACKFRAME, sf_reg[23]));
ASM_SYMBOL(SF_R24,   offsetof(struct STACKFRAME, sf_reg[24]));
ASM_SYMBOL(SF_R25,   offsetof(struct STACKFRAME, sf_reg[25]));
ASM_SYMBOL(SF_R26,   offsetof(struct STACKFRAME, sf_reg[26]));
ASM_SYMBOL(SF_R27,   offsetof(struct STACKFRAME, sf_reg[27]));
ASM_SYMBOL(SF_R28,   offsetof(struct STACKFRAME, sf_reg[28]));
ASM_SYMBOL(SF_R29,   offsetof(struct STACKFRAME, sf_reg[29]));
ASM_SYMBOL(SF_R30,   offsetof(struct STACKFRAME, sf_reg[30]));
ASM_SYMBOL(SF_R31,   offsetof(struct STACKFRAME, sf_reg[31]));

ASM_SYMBOL(SF_SR0,   offsetof(struct STACKFRAME, sf_sr[ 0]));
ASM_SYMBOL(SF_SR1,   offsetof(struct STACKFRAME, sf_sr[ 1]));
ASM_SYMBOL(SF_SR2,   offsetof(struct STACKFRAME, sf_sr[ 2]));
ASM_SYMBOL(SF_SR3,   offsetof(struct STACKFRAME, sf_sr[ 3]));
ASM_SYMBOL(SF_SR4,   offsetof(struct STACKFRAME, sf_sr[ 4]));
ASM_SYMBOL(SF_SR5,   offsetof(struct STACKFRAME, sf_sr[ 5]));
ASM_SYMBOL(SF_SR6,   offsetof(struct STACKFRAME, sf_sr[ 6]));
ASM_SYMBOL(SF_SR7,   offsetof(struct STACKFRAME, sf_sr[ 7]));
ASM_SYMBOL(SF_SR8,   offsetof(struct STACKFRAME, sf_sr[ 8]));
ASM_SYMBOL(SF_SR9,   offsetof(struct STACKFRAME, sf_sr[ 9]));
ASM_SYMBOL(SF_SR10,  offsetof(struct STACKFRAME, sf_sr[10]));
ASM_SYMBOL(SF_SR11,  offsetof(struct STACKFRAME, sf_sr[11]));
ASM_SYMBOL(SF_SR12,  offsetof(struct STACKFRAME, sf_sr[12]));
ASM_SYMBOL(SF_SR13,  offsetof(struct STACKFRAME, sf_sr[13]));
ASM_SYMBOL(SF_SR14,  offsetof(struct STACKFRAME, sf_sr[14]));
ASM_SYMBOL(SF_SR15,  offsetof(struct STACKFRAME, sf_sr[15]));

ASM_SYMBOL(SF_LR,    offsetof(struct STACKFRAME, sf_lr));
ASM_SYMBOL(SF_CR,    offsetof(struct STACKFRAME, sf_cr));
ASM_SYMBOL(SF_XER,   offsetof(struct STACKFRAME, sf_xer));
ASM_SYMBOL(SF_CTR,   offsetof(struct STACKFRAME, sf_ctr));
ASM_SYMBOL(SF_EXC,   offsetof(struct STACKFRAME, sf_exc));
ASM_SYMBOL(SF_DAR,   offsetof(struct STACKFRAME, sf_dar));
ASM_SYMBOL(SF_DSISR, offsetof(struct STACKFRAME, sf_dsisr));

ASM_SYMBOL(SF_SRR0,  offsetof(struct STACKFRAME, sf_srr0));
ASM_SYMBOL(SF_SRR1,  offsetof(struct STACKFRAME, sf_srr1));

ASM_SYMBOL(PCPU_CONTEXT,   offsetof(struct PCPU, context));
ASM_SYMBOL(PCPU_CURTHREAD, offsetof(struct PCPU, curthread));
ASM_SYMBOL(PCPU_TEMP_R1,   offsetof(struct PCPU, temp_r1));

ASM_SYMBOL(THREAD_SF,     offsetof(struct THREAD, md_ctx.sf));
ASM_SYMBOL(THREAD_KSTACK, offsetof(struct THREAD, md_kstack));

ASM_SYMBOL(SC_NUMBER,  offsetof(struct SYSCALL_ARGS, number));
ASM_SYMBOL(SC_ARG1,    offsetof(struct SYSCALL_ARGS, arg1));
ASM_SYMBOL(SC_ARG2,    offsetof(struct SYSCALL_ARGS, arg2));
ASM_SYMBOL(SC_ARG3,    offsetof(struct SYSCALL_ARGS, arg3));
ASM_SYMBOL(SC_ARG4,    offsetof(struct SYSCALL_ARGS, arg4));
ASM_SYMBOL(SC_ARG5,    offsetof(struct SYSCALL_ARGS, arg5));
