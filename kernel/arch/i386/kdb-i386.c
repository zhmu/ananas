#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/thread.h>

extern struct THREAD* kdb_curthread;

void
kdb_cmd_regs(int num_args, char** arg)
{
	if (kdb_curthread == NULL) {
		kprintf("no current thread\n");
		return;
	}
	struct CONTEXT* ctx = &kdb_curthread->md_ctx;
	kprintf("eax=%x ebx=%x ecx=%x edx=%x\n"
	        "esi=%x edi=%x ebp=%x ss:esp=%x:%x\n"
					"cs:eip=%x:%x ds=%x es=%x fs=%x gs=%x\n"
	        "eflags=%x\n",
	 ctx->eax, ctx->ecx, ctx->ebx, ctx->edx,
	 ctx->esi, ctx->edi, ctx->ebp, ctx->ss, ctx->esp,
	 ctx->cs, ctx->eip, ctx->ds, ctx->es, ctx->fs, ctx->gs,
	 ctx->eflags);
}

void
kdb_cmd_trace(int num_args, char** arg)
{
	register uint32_t ebp;
	if (kdb_curthread != NULL) {
		ebp = kdb_curthread->md_ctx.ebp;
	} else {
		__asm __volatile("mov %%ebp, %0" : "=r" (ebp));
	}

	int i = 1;
	while(ebp != 0) {
		kprintf("(%i) 0x%p\n", i++, *(uint32_t*)(ebp + 4));
		ebp = *(uint32_t*)ebp;
	}
}

/* vim:set ts=2 sw=2: */
