/*
 * This file contains platform-specific functions; it mostly consists of
 * calling the BIOS interrupts for the intended functionality and converting
 * values from and to more general values (i.e. LBA instead of CHS)
 */
#include <stdint.h> 
#include <sys/lib.h> 
#include <sys/x86/smap.h>
#include <sys/i386/param.h>	/* for page-size */
#include <loader/diskio.h>
#include <stdio.h> /* for printf */
#include "param.h"

uint32_t x86_pool_pointer = POOL_BASE;

struct REALMODE_REGS {
	uint32_t	eax;
	uint32_t	ebx;
	uint32_t	ecx;
	uint32_t	edx;
	uint32_t	ebp;
	uint32_t	esi;
	uint32_t	edi;
	uint32_t	eflags;
#define EFLAGS_CF	(1 << 0)		/* Carry flag */
#define EFLAGS_PF	(1 << 2)		/* Parity flag */
#define EFLAGS_AF	(1 << 4)		/* Auxiliary carry flag */
#define EFLAGS_ZF	(1 << 6)		/* Zero flag */
#define EFLAGS_SF	(1 << 7)		/* Sign flag */
#define EFLAGS_IF	(1 << 9)		/* Interrupt flag */
#define EFLAGS_DF	(1 << 10)		/* Direction flag */
	uint8_t		interrupt;
} __attribute__((packed));

extern void  realcall();
extern void* rm_regs;
extern void* entry;
extern void* rm_buffer;
#define REALMODE_BUFFER ((uint32_t)&rm_buffer - (uint32_t)&entry)

#undef DEBUG_DISK

struct REALMODE_DISKINFO {
	uint8_t	drive;
	uint16_t cylinders;
	uint8_t heads;
	uint8_t sectors;
};

struct REALMODE_DISKINFO* realmode_diskinfo;

void*
platform_get_memory(uint32_t length)
{
	void* ptr = (void*)x86_pool_pointer;
	//memset(ptr, 0, length);
	x86_pool_pointer += length;
	return ptr;
}

static void
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

	memset(&regs, 0, sizeof(regs));
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

	memset(&regs, 0, sizeof(regs));
	regs.eax = 0x0;			/* keyboard: get keystroke*/
	regs.interrupt = 0x16;
	x86_realmode_call(&regs);

	return regs.eax & 0xff;
}

int
platform_check_key()
{
	struct REALMODE_REGS regs;

	memset(&regs, 0, sizeof(regs));
	regs.eax = 0x01;		/* keyboard: check keystroke */
	regs.interrupt = 0x16;
	x86_realmode_call(&regs);
	/* zero flag is set if we have a keystroke */
	return regs.eflags & EFLAGS_ZF;
}

uint32_t
platform_init_memory_map()
{
	struct REALMODE_REGS regs;
	uint32_t total_kb = 0;

	memset(&regs, 0, sizeof(regs));
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
		uint64_t base = (uint64_t)sme->base_hi << 32 | sme->base_lo;
		uint64_t length  = (uint64_t)sme->len_hi << 32  | sme->len_lo;
		base    = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		length &= ~(PAGE_SIZE - 1);

		/* Store it XXX */
		total_kb += length >> 10;
	
	} while (regs.ebx != 0);

	return total_kb;
}

int
platform_init_disks()
{
	struct REALMODE_REGS regs;
	int numdisks = 0;

	realmode_diskinfo = platform_get_memory(sizeof(struct REALMODE_DISKINFO) * MAX_DISKS);
	for (uint8_t drive = 0x80; drive < 0x80 + MAX_DISKS; drive++) {
		memset(&regs, 0, sizeof(regs));
		regs.eax = 0x0800;		/* disk: get drive parameters */
		regs.edx = drive;
		regs.interrupt = 0x13;
		x86_realmode_call(&regs);
		if (regs.eflags & EFLAGS_CF)
			break;
		
		struct REALMODE_DISKINFO* dskinfo = &realmode_diskinfo[numdisks];
		dskinfo->drive = drive;
		dskinfo->cylinders = ((regs.ecx & 0xff00) >> 8) | ((regs.ecx & 0xc0) << 2);
		dskinfo->heads = (regs.edx & 0xff00) >> 8;
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
		memset(&regs, 0, sizeof(regs));
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

/* vim:set ts=2 sw=2: */
