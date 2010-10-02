/*
 * This file contains platform-specific functions; it mostly consists of
 * calling the BIOS interrupts for the intended functionality and converting
 * values from and to more general values (i.e. LBA instead of CHS)
 */
#include <ananas/types.h> 
#include <ananas/x86/smap.h>
#include <ananas/i386/param.h>	/* for page-size */
#include <ananas/bootinfo.h>
#include <loader/diskio.h>
#include <loader/lib.h>
#include <loader/platform.h>
#include <loader/x86.h>
#include "param.h"

uint32_t x86_pool_pointer = POOL_BASE;
static uint32_t x86_pool_left = 0;
struct SMAP_ENTRY* x86_smap = NULL; 
int x86_smap_entries = 0;

#undef DEBUG_POOL
#undef DEBUG_DISK

struct REALMODE_DISKINFO {
	uint8_t	drive;
	uint16_t cylinders;
	uint8_t heads;
	uint8_t sectors;
};

struct REALMODE_DISKINFO* realmode_diskinfo;
uint32_t x86_realmode_worksp;

void*
platform_get_memory(uint32_t length)
{
	void* ptr = (void*)x86_pool_pointer;
	memset(ptr, 0, length);
	x86_pool_pointer += length;
	if (x86_smap_entries > 0)
		x86_pool_left -= length;
	return ptr;
}

uint32_t
platform_get_memory_left()
{
	return x86_pool_left;
}

void
x86_realmode_init(struct REALMODE_REGS* regs)
{
	memset(regs, 0, sizeof(struct REALMODE_REGS));
	regs->ds = (CODE_BASE >> 4);
	regs->es = (CODE_BASE >> 4);
	regs->ss = 0;
	regs->esp = (uint32_t)&rm_stack;
	x86_realmode_worksp = (uint32_t)&rm_stack;
}

void
x86_realmode_push16(struct REALMODE_REGS* regs, uint16_t value)
{
	x86_realmode_worksp -= 2;
	regs->esp -= 2;
	*(uint16_t*)x86_realmode_worksp = value;
}

void
x86_realmode_call(struct REALMODE_REGS* regs)
{
	memcpy(&rm_regs, regs, sizeof(struct REALMODE_REGS));
	realcall();
	memcpy(regs, &rm_regs, sizeof(struct REALMODE_REGS));
}

void
platform_putch(uint8_t ch)
{
	struct REALMODE_REGS regs;

	x86_realmode_init(&regs);
	regs.eax = 0x0e00 | ch;		/* video: tty output */
	regs.interrupt = 0x10;
	x86_realmode_call(&regs);

	/* XXX this a hack */
	if (ch == '\n')
		platform_putch('\r');
}

int
platform_getch()
{
	struct REALMODE_REGS regs;

	x86_realmode_init(&regs);
	regs.eax = 0x0;			/* keyboard: get keystroke*/
	regs.interrupt = 0x16;
	x86_realmode_call(&regs);

	return regs.eax & 0xff;
}

int
platform_check_key()
{
	struct REALMODE_REGS regs;

	x86_realmode_init(&regs);
	regs.eax = 0x01;		/* keyboard: check keystroke */
	regs.interrupt = 0x16;
	x86_realmode_call(&regs);
	/* zero flag is set if we have a keystroke */
	return regs.eflags & EFLAGS_ZF;
}

size_t
platform_init_memory_map()
{
	struct REALMODE_REGS regs;
	size_t total_kb = 0;

	/*
	 * Find a place to locate our memory. Don't worry about the size, we will
	 * relocate the base memory pointer once we know how much memory there is
	 * anyway.
 	 */
	x86_smap = platform_get_memory(0);
	x86_smap_entries = 0;

	x86_realmode_init(&regs);
	regs.ebx = 0;
	regs.interrupt = 0x15;
	do {
		/*
	 	 * Use the BIOS to obtain the E820 memory map.
		 */
		regs.eax = 0xe820;
		regs.edx = 0x534d4150;		/* SMAP */
		regs.ecx = 20;
		regs.edi = REALMODE_BUFFER;
		x86_realmode_call(&regs);
		if (regs.eax != 0x534d4150)
			break;

		/* ignore anything that is not memory */
		struct SMAP_ENTRY* sme = (struct SMAP_ENTRY*)&rm_buffer;
		if (sme->type != SMAP_TYPE_MEMORY)
			continue;

		/* Round base up to a page, and length down a page if needed */
		uint64_t length  = (uint64_t)sme->len_hi << 32  | sme->len_lo;
		length &= ~(PAGE_SIZE - 1);

		/* Store it */
		memcpy(&x86_smap[x86_smap_entries], sme, sizeof(struct SMAP_ENTRY));
		x86_smap_entries++;
		total_kb += length >> 10;
	} while (regs.ebx != 0);

	/* Locate the loader pool at the far end of the memory */
	for (int i = x86_smap_entries - 1; i >= 0; i--) {
		struct SMAP_ENTRY* sme = (struct SMAP_ENTRY*)&x86_smap[i];

		/* Skip anything not in the 4GB range */
		if (sme->base_hi > 0)
			continue;
		uint64_t length  = (uint64_t)sme->len_hi << 32  | sme->len_lo;
		length &= ~(PAGE_SIZE - 1);

		/* XXX Steal 1MB for now, ensure it doesn't cross 4GB boundary */
		uint64_t addr = sme->base_lo + length - (1 * 1024 * 1024);

		x86_pool_pointer = (uint32_t)addr;
		x86_pool_left = (uint64_t)(sme->base_lo + length) - addr;
#ifdef DEBUG_POOL
		printf("memory pool at 0x%x, %u bytes left\n", x86_pool_pointer, x86_pool_left);
#endif
		break;
	}

	return total_kb;
}

int
platform_init_disks()
{
	struct REALMODE_REGS regs;
	int numdisks = 0;

	realmode_diskinfo = platform_get_memory(sizeof(struct REALMODE_DISKINFO) * MAX_DISKS);
	for (uint8_t drive = 0x80; drive < 0x80 + MAX_DISKS; drive++) {
		x86_realmode_init(&regs);
		regs.eax = 0x0800;		/* disk: get drive parameters */
		regs.edx = drive;
		regs.interrupt = 0x13;
		x86_realmode_call(&regs);
		if (regs.eflags & EFLAGS_CF)
			break;
		
		struct REALMODE_DISKINFO* dskinfo = &realmode_diskinfo[numdisks];
		dskinfo->drive = drive;
		dskinfo->cylinders = ((regs.ecx & 0xff00) >> 8) | ((regs.ecx & 0xc0) << 2);
		dskinfo->heads = ((regs.edx & 0xff00) >> 8) + 1;
		dskinfo->sectors = (regs.ecx & 0x3f);
#ifdef DEBUG_DISK
		printf("disk %u: drive 0x%x, c/h/s = %u/%u/%u\n",
		 drive, dskinfo->drive,
		 dskinfo->cylinders, dskinfo->heads, dskinfo->sectors);
#endif
		numdisks++;
	}

	return numdisks;
}

int
platform_read_disk(int disk, uint32_t lba, void* buffer, int num_bytes)
{
	struct REALMODE_DISKINFO* dskinfo = &realmode_diskinfo[disk];
	if (disk >= MAX_DISKS || dskinfo->cylinders == 0 || dskinfo->heads == 0 || dskinfo->sectors == 0)
		return 0;
	if ((num_bytes % SECTOR_SIZE) != 0)
		return 0;

	int num_read = 0;
	while (num_bytes > 0) {
		uint8_t cylinder = (lba / dskinfo->sectors) / dskinfo->heads;
		uint8_t head = (lba / dskinfo->sectors) % dskinfo->heads;
		uint8_t sector = (lba % dskinfo->sectors) + 1;

#ifdef DEBUG_DISK
		printf("disk %u, lba %u => c/h/s %u/%u/%u\n", disk, lba, cylinder, head, sector);
#endif

		struct REALMODE_REGS regs;
		x86_realmode_init(&regs);
		regs.eax = 0x0201;		/* disk: read one sector into memory */
		regs.ebx = REALMODE_BUFFER;
		regs.ecx = ((cylinder & 0xff) << 8) | ((cylinder & 0xc0) << 2) | sector;
		regs.edx = dskinfo->drive | (head << 8);
		regs.interrupt = 0x13;
		x86_realmode_call(&regs);
		if (regs.eflags & EFLAGS_CF)
			break;

		int chunk_len = (num_bytes > SECTOR_SIZE ? SECTOR_SIZE : num_bytes);
		memcpy(buffer, &rm_buffer, chunk_len);
		buffer += chunk_len; num_read += chunk_len; num_bytes -= chunk_len;
	}

	return num_read;
}

void
platform_reboot()
{
	/*
	 * We just call the reset handler; this will never return.
	 */
	struct REALMODE_REGS regs;
	x86_realmode_init(&regs);
	regs.cs = 0xf000;
	regs.ip = 0xfff0;
	x86_realmode_call(&regs);
}

void
platform_cleanup()
{
	platform_cleanup_netboot();
}

void
platform_exec(uint64_t entry, struct BOOTINFO* bootinfo)
{
	typedef void kentry(int, void*, int);
	uint32_t entry32 = (entry & 0x0fffffff);
	((kentry*)entry32)(BOOTINFO_MAGIC_1, bootinfo, BOOTINFO_MAGIC_2);
}

/* vim:set ts=2 sw=2: */
