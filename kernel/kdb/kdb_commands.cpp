#include <ananas/kdb.h>
#include <ananas/thread.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/irq.h>
#include <ananas/bootinfo.h>
#include <ananas/lib.h>
#include <ananas/vfs.h>
#include <ananas/mm.h>

KDB_COMMAND(bootinfo, NULL, "Display boot info")
{
	if (bootinfo == NULL) {
		kprintf("no bootinfo supplied by loader\n");
		return;
	}
	kprintf("modules location   : %x\n",
	 bootinfo->bi_modules);
	kprintf("memorymap location: %x - %x\n",
	 bootinfo->bi_memory_map_addr,
	 bootinfo->bi_memory_map_addr + bootinfo->bi_memory_map_size);
}

/* vim:set ts=2 sw=2: */
