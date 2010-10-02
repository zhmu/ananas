/*
 * This file takes care of all I/O activity. For performance reasons, it uses a
 * sorted cache to prevent reading blocks over and over.
 *
 * All blocks are hard-coded to 512 bytes, which means this is the only amount
 * of data that can be read (successive reads are needed to work with
 * filesystems that have a larger blocksize) - this is done because it keeps
 * the I/O layer much simpler.
 *
 * All blocks in the cache are kept sorted in a most-recent-first fashion, and
 * the used blocks are always at the front. This makes it easy to locate a
 * new block to use and to evict used blocks.
 *
 */
#include <ananas/types.h>
#include <loader/diskio.h>
#include <loader/lib.h>
#include <loader/platform.h>
#include <mbr.h>

#define MAX_DISK_DEVICE_NAME 8
#define MAX_DISK_DEVICES 16

struct DISK_DEVICE {
	char     name[MAX_DISK_DEVICE_NAME];
	int      device;
	uint32_t start_lba;
	uint32_t length;
};

#define DISK_CACHE_SIZE 8192
#define NUM_CACHE_ENTRIES (DISK_CACHE_SIZE / sizeof(struct CACHE_ENTRY))

struct DISK_DEVICE* disk_device;
struct CACHE_ENTRY* disk_cache;

static int num_disk_devices = 0;
static int diskio_hits      = 0;
static int diskio_misses    = 0;
static int diskio_used      = 0;

static void
diskio_place_head(struct CACHE_ENTRY* entry)
{
	if (entry == disk_cache)
		return;
	if (entry->prev != NULL)
		entry->prev->next = entry->next;
	if (entry->next != NULL)
		entry->next->prev = entry->prev;
	entry->prev = disk_cache->prev;
	entry->next = disk_cache;
	disk_cache->prev = entry;
	disk_cache = entry;
}

static int
diskio_find_cache(int device, uint32_t lba, struct CACHE_ENTRY** centry)
{
	struct CACHE_ENTRY* entry = disk_cache;
	for (; entry->next != NULL; entry = entry->next) {
		if (entry->device == device && entry->lba == lba) {
			/* Got it */
			diskio_place_head(entry);
			*centry = entry;
			diskio_hits++;
			return 1;
		}

		/* If this block is used, it's not the block we are looking for */
		if (entry->device != -1) {
			continue;
		}

		/*
		 * Found an unused block - we know that we've reached the end of the
		 * chain, so we can just use whatever block is here.
		 */
		diskio_place_head(entry);
		*centry = entry;
		diskio_used++;
		return 0;
	}

	/* List is full (entry->next is NULL) - sacrifice this final block */
	*centry = entry;
	diskio_place_head(entry);
	diskio_misses++;
	return 0;
}

static int
diskio_add_device(char* name, int device, uint32_t start_lba, uint32_t length)
{
	if (num_disk_devices >= MAX_DISK_DEVICES)
		return 0;
	struct DISK_DEVICE* disk = &disk_device[num_disk_devices];
	strcpy(disk->name, name);
	disk->device = device;
	disk->start_lba = start_lba;
	disk->length = length;
	num_disk_devices++;
	return 1;
}

struct CACHE_ENTRY*
diskio_read(int disknum, uint32_t lba)
{
	if (disknum < 0 || disknum >= MAX_DISK_DEVICES)
		return 0;

	struct DISK_DEVICE* disk = &disk_device[disknum];
	struct CACHE_ENTRY* centry;
	lba += disk->start_lba;
	if (diskio_find_cache(disk->device, lba, &centry))
		return centry;

	int tries = 5;
	while(tries > 0) {
		if (platform_read_disk(disk->device, lba, centry->data, SECTOR_SIZE) == SECTOR_SIZE) {
			centry->device = disk->device; centry->lba = lba;
			return centry;
		}
		tries--;
	}
	if (!tries) {
		printf("diskio: read error\n");
		return NULL;
	}
}

unsigned int
diskio_init()
{
	unsigned char buf[SECTOR_SIZE];
	char newname[MAX_DISK_DEVICE_NAME];

	disk_device = platform_get_memory(sizeof(struct DISK_DEVICE) * MAX_DISK_DEVICES);
	disk_cache = platform_get_memory(DISK_CACHE_SIZE);
	for (int i = 0; i < NUM_CACHE_ENTRIES; i++) {
		if (i > 0)
			disk_cache[i].prev = &disk_cache[i - 1];
		else
			disk_cache[i].prev = NULL;
		if (i < NUM_CACHE_ENTRIES - 1)
			disk_cache[i].next = &disk_cache[i + 1];
		else
			disk_cache[i].next = NULL;
		disk_cache[i].device = -1;
	}

	/* Detect disks/slices. Note that we only support x86 MBR's for now */
	for (int disk = 0; disk < MAX_DISKS; disk++) {
		if (platform_read_disk(disk, 0, buf, SECTOR_SIZE) != SECTOR_SIZE)
			continue;

		/*
		 * We can read the first sector, so this disk is available.
		 */
		sprintf(newname, "disk%u", disk);
		diskio_add_device(newname, disk, 0, 0);

		/* Check the MBR signature. If this does not match, do not parse it */
		struct MBR* mbr = (struct MBR*)buf;
		if (((mbr->signature[1] << 8) | mbr->signature[0]) != MBR_SIGNATURE)
			continue;

		/* All any entries one by one */
		for (int n = 0; n < MBR_NUM_ENTRIES; n++) {
			struct MBR_ENTRY* entry = &mbr->entry[n];
			/* Skip any entry with an invalid bootable flag or an ID of zero */
			if (entry->bootable != 0 && entry->bootable != 0x80)
				continue;
			if (entry->system_id == 0)
				continue;

			uint32_t first_sector =
				entry->lba_start[0]       | entry->lba_start[1] << 8 |
				entry->lba_start[2] << 16 | entry->lba_start[3] << 24;
			uint32_t size =
				entry->lba_size[0]       | entry->lba_size[1] << 8 |
				entry->lba_size[2] << 16 | entry->lba_size[3] << 24;
			if (first_sector == 0 || size == 0)
				continue;

			sprintf(newname, "disk%u%c", disk, n + 'a');
			diskio_add_device(newname, disk, first_sector, size);
		}
	}

	printf(">> Found following devices: ");
	for (int i = 0; i < num_disk_devices; i++) {
		struct DISK_DEVICE* disk = &disk_device[i];
		printf("%s ", disk->name);
	}
	platform_putch('\n');
	return num_disk_devices;
}

const char*
diskio_get_name(int device)
{
	if (num_disk_devices >= MAX_DISK_DEVICES)
		return "(none)";
	return disk_device[device].name;
}

void
diskio_stats()
{
	printf("Cache entries: %u used / %u total\n",
	 diskio_used, NUM_CACHE_ENTRIES);
	printf("Cache utilization: %u hits, %u misses\n",
	 diskio_hits, diskio_misses);
}

/* vim:set ts=2 sw=2: */
