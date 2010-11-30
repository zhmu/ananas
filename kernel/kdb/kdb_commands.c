#include <ananas/kdb.h>
#include <ananas/thread.h>
#include <ananas/threadinfo.h>
#include <ananas/bio.h>
#include <ananas/handle.h>
#include <ananas/bootinfo.h>
#include <ananas/lib.h>
#include <ananas/vfs.h>
#include <ananas/mm.h>

void
kdb_cmd_threads(int num_args, char** arg)
{
	thread_dump();
}

void
kdb_cmd_thread(int num_args, char** arg)
{
	if (num_args != 2) {
		kprintf("need an argument\n");
		return;
	}

	char* ptr;
	addr_t addr = (addr_t)strtoul(arg[1], &ptr, 16);
	if (*ptr != '\0') {
		kprintf("parse error at '%s'\n", ptr);
		return;
	}

	struct THREAD* thread = (void*)addr;
	kprintf("arg          : '%s'\n", thread->threadinfo->ti_args);
	kprintf("flags        : 0x%x\n", thread->flags);
	kprintf("terminateinfo: 0x%x\n", thread->terminate_info);
	kprintf("mappings:\n");
	if (!DQUEUE_EMPTY(&thread->mappings)) {
		DQUEUE_FOREACH(&thread->mappings, tm, struct THREAD_MAPPING) {
			kprintf("   flags      : 0x%x\n", tm->flags);
			kprintf("   address    : 0x%x - 0x%x\n", tm->start, tm->start + tm->len);
			kprintf("   length     : %u\n", tm->len);
			kprintf("   backing ptr: 0x%x - 0x%x\n", tm->ptr, tm->ptr + tm->len);
			kprintf("\n");
		}
	}
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

void
kdb_cmd_handle(int num_args, char** arg)
{
	if (num_args != 2) {
		kprintf("need an argument\n");
		return;
	}

	char* ptr;
	addr_t addr = (addr_t)strtoul(arg[1], &ptr, 16);
	if (*ptr != '\0') {
		kprintf("parse error at '%s'\n", ptr);
		return;
	}

	struct HANDLE* handle = (void*)addr;
	kprintf("type          : %u\n", handle->type);
	kprintf("owner thread  : 0x%x\n", handle->thread);
	kprintf("waiters:\n");
	for (unsigned int i = 0; i < HANDLE_MAX_WAITERS; i++) {
		if (handle->waiters[i].thread == NULL)
			continue;
		kprintf("   wait thread    : 0x%x\n", handle->waiters[i].thread);
		kprintf("   wait event     : %u\n", handle->waiters[i].event);
		kprintf("   event mask     : %u\n", handle->waiters[i].event_mask);
		kprintf("   event reported : %u\n", handle->waiters[i].event_reported);
		kprintf("   result         : %u\n", handle->waiters[i].result);
	}

	switch(handle->type) {
		case HANDLE_TYPE_FILE: {
			kprintf("file handle specifics:\n");
			kprintf("   offset         : %u\n", handle->data.vfs_file.offset); /* XXXSIZE */
			kprintf("   inode          : 0x%x\n", handle->data.vfs_file.inode);
			kprintf("   device         : 0x%x\n", handle->data.vfs_file.device);
			break;
		}
		case HANDLE_TYPE_THREAD: {
			kprintf("thread handle specifics:\n");
			kprintf("   thread         : 0x%x\n", handle->data.thread);
			break;
		}
		case HANDLE_TYPE_MEMORY: {
			kprintf("memory handle specifics:\n");
			kprintf("   address        : 0x%x\n", handle->data.memory.addr);
			kprintf("   size           : %u\n", handle->data.memory.length);
			break;
		}
	}
}

void
kdb_cmd_fsinfo(int num_args, char** arg)
{
	vfs_dump();
}

/* vim:set ts=2 sw=2: */
