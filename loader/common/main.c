#include <ananas/types.h>
#include <loader/diskio.h>
#include <loader/lib.h>
#include <loader/platform.h>
#include <loader/vfs.h>
#include <loader/elf.h>

#if defined(__i386__)
#define PLATFORM "x86"
#elif defined(_ARCH_PPC)
#define PLATFORM "powerpc"
#endif

typedef void kentry(void);

void interact();

int
main()
{
	printf("Ananas/"PLATFORM" Loader: ");
	printf("%u KB memory", platform_init_memory_map());
	printf(" / %u disk(s)\n", platform_init_disks());

	int num_disk_devices = diskio_init();
	vfs_init();

	int netbooting = platform_init_netboot();

	/*
	 * Find something we can mount as boot filesystem. We prefer netbooting if
	 * it's available.
	 */
	int got_root = 0;
	if (netbooting) {
		const char* type;
		if (vfs_mount(-1, &type)) {
			printf(">> Mounted 'network' as '%s'\n", type);
			got_root++;
		}
	}

	if (!got_root) {
		unsigned int dev = 0;
		for (dev = 0; dev < num_disk_devices; dev++) {
			const char* type;
			if (vfs_mount(dev, &type)) {
				printf(">> Mounted '%s' as '%s'\n", diskio_get_name(dev), type);
				got_root++;
				break;
			}
		}
	}

	if (!got_root)
		printf("WARNING: no usuable disks found!\n");

	interact();

	/* NOTREACHED */

	return 0;
}

/* vim:set ts=2 sw=2: */
