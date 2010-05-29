#include <stdio.h>
#include <loader/diskio.h>
#include <loader/platform.h>
#include <loader/vfs.h>
#include <loader/elf.h>

typedef void kentry(void);

const char* kernels[] = {
	"kernel",
	NULL
};

int
main()
{
	printf("Ananas/x86 Loader: ");
	printf("%u KB memory", platform_init_memory_map());
	printf(" / %u disk(s)\n", platform_init_disks());

	int num_disk_devices = diskio_init();
	vfs_init();

	int netbooting = platform_init_netboot();

	/*
	 * Find something we can mount as boot filesystem.
	 */
	unsigned int dev = 0;
	for (dev = 0; dev < num_disk_devices; dev++) {
		const char* type;
		if (vfs_mount(dev, &type)) {
			printf(">> Mounted '%s' as '%s'\n", diskio_get_name(dev), type);
			break;
		}
	}

	if (dev == num_disk_devices)
		printf("WARNING: no usuable disks found!\n");

	/*
	 * Try our list of kernels.
	 */
	for (const char** kernel = kernels; *kernel != NULL; kernel++) {
		printf("Trying '%s'...", *kernel);
		if (!vfs_open(*kernel)) {
			printf(" failed, cannot open\n");
			continue;
		}

		uint64_t entry;
		if (!elf_load(&entry)) {
			printf(" failed, cannot load\n");
			continue;
		}

		uint32_t entry32 = (entry & 0x0fffffff);
		printf(" ok, launching from 0x%x!\n", entry32);
		((kentry*)entry32)();

		/* Why are we here? */
	}

	/* XXX should present some interactive kernel choser here... */
	while (1) {
		int i = platform_getch();
		platform_putch(i);
	}
	return 0;
}

/* vim:set ts=2 sw=2: */
