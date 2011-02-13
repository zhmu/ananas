#include <ananas/kdb.h>
#include <ananas/thread.h>
#include <ananas/threadinfo.h>
#include <ananas/bio.h>
#include <ananas/handle.h>
#include <ananas/irq.h>
#include <ananas/bootinfo.h>
#include <ananas/lib.h>
#include <ananas/vfs.h>
#include <ananas/mm.h>

void
kdb_cmd_bootinfo(int num_args, char** arg)
{
	if (bootinfo == NULL) {
		kprintf("no bootinfo supplied by loader\n");
		return;
	}
	kprintf("kernel location   : %x - %x\n",
	 bootinfo->bi_kernel_addr,
	 bootinfo->bi_kernel_addr + bootinfo->bi_kernel_size);
	kprintf("ramdisk location  : %x - %x\n",
	 bootinfo->bi_ramdisk_addr,
	 bootinfo->bi_ramdisk_addr + bootinfo->bi_ramdisk_size);
	kprintf("memorymap location: %x - %x\n",
	 bootinfo->bi_memory_map_addr,
	 bootinfo->bi_memory_map_addr + bootinfo->bi_memory_map_size);
}

void
kdb_cmd_memory(int num_args, char** arg)
{
	size_t avail, total;
	kmem_stats(&avail, &total);

	kprintf("memory: %u KB available of %u KB total\n",
	 avail / 1024, total / 1024);
}

/* vim:set ts=2 sw=2: */
