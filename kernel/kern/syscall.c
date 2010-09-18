#include <ananas/syscall.h>
#include <ananas/stat.h>
#include <ananas/handle.h>
#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <syscalls.h>

register_t
syscall(struct SYSCALL_ARGS* a)
{
	//kprintf("syscall, CPU=0x%x, no=%u,a1=0x%x,a2=0x%x,a3=0x%x,a4=0x%x,a5=0x%x\n", PCPU_GET(cpuid), a->number, a->arg1, a->arg2, a->arg3, a->arg4, a->arg5);
	switch(a->number) {
#include "syscalls.inc.c"
		default:
			kprintf("syscall, CPU=0x%x, no=%u,a1=0x%x,a2=0x%x,a3=0x%x,a4=0x%x,a5=0x%x\n",
			 PCPU_GET(cpuid),
			 a->number, a->arg1, a->arg2, a->arg3, a->arg4, a->arg5);
			break;
	}
	return 1;
}

/* vim:set ts=2 sw=2: */
