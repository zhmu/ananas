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

ASM_SYMBOL(PCPU_FPUCTX, offsetof(struct PCPU, fpu_context));
