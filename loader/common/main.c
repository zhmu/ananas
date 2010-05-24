#include <stdio.h>
#include <loader/diskio.h>
#include <loader/platform.h>

int
main()
{
	printf("Ananas/x86 Loader: ");
	printf("%u KB memory", platform_init_memory_map());
	printf(" / %u disk(s)\n", platform_init_disks());
	diskio_init();

	while (1) {
		int i = platform_getch();
		platform_putch(i);
	}

	return 0;
}
