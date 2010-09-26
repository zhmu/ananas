#include <ananas/kdb.h>
#include <ananas/thread.h>
#include <ananas/bio.h>
#include <ananas/bootinfo.h>
#include <ananas/lib.h>

void
kdb_cmd_threads(int num_args, char** arg)
{
	thread_dump();
}

void
kdb_cmd_bio(int num_args, char** arg)
{
	bio_dump();
}

void
kdb_cmd_bootinfo(int num_args, char** arg)
{
	if (bootinfo == NULL) {
		kprintf("no bootinfo supplied by loader\n");
		return;
	}
	kprintf("kernel location : %x - %x\n",
	 bootinfo->bi_kernel_firstaddr,
	 bootinfo->bi_kernel_firstaddr + bootinfo->bi_kernel_size);
	kprintf("ramdisk location: %x - %x\n",
	 bootinfo->bi_ramdisk_firstaddr,
	 bootinfo->bi_ramdisk_firstaddr + bootinfo->bi_ramdisk_size);
}

/* vim:set ts=2 sw=2: */
