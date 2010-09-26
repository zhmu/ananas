#include <loader/lib.h>
#include <loader/diskio.h>
#include <loader/platform.h>
#include <ofw.h>

struct OFW_DISKINFO {
	char device[128];
	ofw_cell_t ihandle;
};

static struct OFW_DISKINFO* ofw_diskinfo;
static int ofw_curdisk = -1;

void*
platform_get_memory(uint32_t length)
{
	return ofw_heap_alloc(length);
}

void
platform_putch(uint8_t ch)
{
	ofw_putch(ch);
}

int
platform_getch()
{
	return ofw_getch();
}

int
platform_init_disks()
{
	int numdisks = 0;

	ofw_diskinfo = (struct OFW_DISKINFO*)platform_get_memory(sizeof(struct OFW_DISKINFO) * MAX_DISKS);
	for(int i = 0; i < MAX_DISKS; i++)
		ofw_diskinfo[i].device[0] = '\0';

	/*
	 * In order to have some idea of the disks in this machine, we look at the
 	 * /aliases list and see if any of the devices is a block device.
	 */
	ofw_cell_t aliases = ofw_finddevice("/aliases");
	char current[32], name[32];
	name[0] = '\0';
	while(1) {
		if (numdisks == MAX_DISKS) {
			printf("Warning: already found %u disks, will not parse more\n", numdisks);
			break;
		}

		/* Fetch the next item */
		strcpy(current, name);
		if (ofw_nextprop(aliases, current, name) != 1)
			break;

		/* Found something; obtain the name */
		char realname[128];
		if (ofw_getprop(aliases, name, realname, sizeof(realname)) == sizeof(realname))
			continue;

		ofw_cell_t device = ofw_finddevice(realname);
		char devicetype[32];
		int dtlen = ofw_getprop(device, "device_type", devicetype, sizeof(devicetype));
		if (dtlen <= 0 || dtlen == sizeof(devicetype))
			continue;

		/* Skip non-block devices */
		if (strcmp(devicetype, "block") != 0)
			continue;

		strcpy(ofw_diskinfo[numdisks].device, realname);
		ofw_diskinfo[numdisks].ihandle = 0;
		numdisks++;
	}

	return numdisks;
}

int
platform_read_disk(int disk, uint32_t lba, void* buffer, int num_bytes)
{
	if (disk < 0 || disk >= MAX_DISKS)
		return 0;

	struct OFW_DISKINFO* odi = &ofw_diskinfo[disk];
	if (odi->device[0] == '\0')
		return 0;

	/* If this is not the current disk, open it */
	if (ofw_curdisk != disk) {
		if (ofw_curdisk != -1) {
			ofw_close(ofw_diskinfo[ofw_curdisk].ihandle);
			ofw_diskinfo[ofw_curdisk].ihandle = 0;
			ofw_curdisk = -1;
		}

		ofw_cell_t ihandle = ofw_open(ofw_diskinfo[disk].device);
		if (ihandle == 0)
			return 0;
	
		ofw_diskinfo[disk].ihandle = ihandle;
		ofw_curdisk = disk;
	}


	if (ofw_seek(odi->ihandle, lba * SECTOR_SIZE) < 0)
		return 0;

	return ofw_read(odi->ihandle, buffer, num_bytes);
}

void
platform_reboot()
{
	ofw_exit();
}

void
platform_cleanup()
{
}

int
platform_init_netboot()
{
	return 0;
}

uint64_t
platform_init_memory_map()
{
	return ofw_get_memory_size();
}

void
platform_init(int r3, int r4, unsigned int r5)
{
	ofw_init();

	main();
}

void
platform_exec(uint64_t entry, struct BOOTINFO* bootinfo)
{
	typedef void kentry(uint32_t r3, uint32_t r4, uint32_t r5);

	/*
	 * XXX This will only work with powerpc32 binaries yet. Note that we
	 *     add the OFW entry point so that it's easier to obtain and use.
	 */
	uint32_t entry32 = (entry & 0xffffffff);
	((kentry*)entry32)(BOOTINFO_MAGIC_1, bootinfo, (uint32_t)ofw_entry);
}

/* vim:set ts=2 sw=2: */
