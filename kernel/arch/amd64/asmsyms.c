#include "asmsymbols.h"
#include "machine/frame.h"

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
ASM_SYMBOL(SF_RIP,    offsetof(struct STACKFRAME, sf_rip));
