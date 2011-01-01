#include <machine/frame.h>
#include <machine/pcpu.h>
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

ASM_SYMBOL(CTX_EAX,    offsetof(struct CONTEXT, eax));
ASM_SYMBOL(CTX_EBX,    offsetof(struct CONTEXT, ebx));
ASM_SYMBOL(CTX_ECX,    offsetof(struct CONTEXT, ecx));
ASM_SYMBOL(CTX_EDX,    offsetof(struct CONTEXT, edx));
ASM_SYMBOL(CTX_ESI,    offsetof(struct CONTEXT, esi));
ASM_SYMBOL(CTX_EDI,    offsetof(struct CONTEXT, edi));
ASM_SYMBOL(CTX_EBP,    offsetof(struct CONTEXT, ebp));
ASM_SYMBOL(CTX_ESP,    offsetof(struct CONTEXT, esp));
ASM_SYMBOL(CTX_EIP,    offsetof(struct CONTEXT, eip));
ASM_SYMBOL(CTX_CS,     offsetof(struct CONTEXT, cs));
ASM_SYMBOL(CTX_DS,     offsetof(struct CONTEXT, ds));
ASM_SYMBOL(CTX_ES,     offsetof(struct CONTEXT, es));
ASM_SYMBOL(CTX_FS,     offsetof(struct CONTEXT, fs));
ASM_SYMBOL(CTX_GS,     offsetof(struct CONTEXT, gs));
ASM_SYMBOL(CTX_SS,     offsetof(struct CONTEXT, ss));
ASM_SYMBOL(CTX_CR3,    offsetof(struct CONTEXT, cr3));
ASM_SYMBOL(CTX_EFLAGS, offsetof(struct CONTEXT, eflags));
ASM_SYMBOL(CTX_ESP0,   offsetof(struct CONTEXT, esp0));

ASM_SYMBOL(PCPU_CONTEXT, offsetof(struct PCPU, context));
ASM_SYMBOL(PCPU_FPUCTX, offsetof(struct PCPU, fpu_context));
ASM_SYMBOL(PCPU_TICKCOUNT, offsetof(struct PCPU, tickcount));
