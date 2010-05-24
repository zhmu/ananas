#include <sys/types.h>
#include <loader/diskio.h>
#include <loader/platform.h>
#include <mbr.h>
#include <stdio.h>
#include <string.h>

#define MAX_DISK_DEVICE_NAME 8
#define MAX_DISK_DEVICES 16

struct DISK_DEVICE {
	char     name[MAX_DISK_DEVICE_NAME];
	int      device;
	uint32_t start_lba;
	uint32_t length;
};

struct DISK_DEVICE* disk_device;

int num_disk_devices = 0;

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

int
diskio_init()
{
	unsigned char buf[SECTOR_SIZE];
	char newname[MAX_DISK_DEVICE_NAME];

	disk_device = platform_get_memory(sizeof(struct DISK_DEVICE) * MAX_DISK_DEVICES);

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
}

/* vim:set ts=2 sw=2: */
