#ifndef __DISKIO_H__
#define __DISKIO_H__

#define MAX_DISKS 8
#define SECTOR_SIZE 512

struct CACHE_ENTRY {
	int      device;
	uint32_t lba;
	uint8_t  data[SECTOR_SIZE];
};

unsigned int diskio_init();
struct CACHE_ENTRY* diskio_read(int disknum, uint32_t lba);
const char* diskio_get_name(int device);

#endif /* __DISKIO_H__ */
