#include <sys/types.h>

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

struct BOOTINFO;
struct LOADER_ELF_INFO;

/* Retrieve a block of length bytes available for use - must be zeroed out */
void* platform_get_memory(uint32_t length);

/* Retrieve the amount of platform memory available, or 0 if unknown */
uint32_t platform_get_memory_left();

/* Display a single charachter */
void platform_putch(uint8_t ch);

/* Get a keystroke from the input provider, blocks if nothing is available */
int platform_getch();

/* Checks whether a keystroke is available */
int platform_check_key();

/* Retrieve the platform's memory map for passing to the kernel. Returns KB's of memory available */
size_t platform_init_memory_map();

/* Initialize disk subsystem. Returns number of disks available */
int platform_init_disks();

/* Initialze disk slices. Returns number of slices total */
int platform_init_slices();

/* Reads a block from a disk to buffer. Returns numbers of bytes read */
int platform_read_disk(int disk, uint32_t lba, void* buffer, int num_bytes);

/* Initialize netboot environment, if applicable. Returns non-zero if netbooting */
int platform_init_netboot();

/* Cleans up the environment - must be called before launching kernel */
void platform_cleanup();

/* Cleans up the netboot environment, if applicable. */
void platform_cleanup_netboot();

/* Reboots the system */
void platform_reboot();

/* Launches a loaded kernel */
void platform_exec(struct LOADER_ELF_INFO* loadinfo, struct BOOTINFO* bootinfo);

/* Checks whether the platform can handle a certain bit executable */
int platform_is_numbits_capable(int bits);

/* Establish a memory mapping for a piece of memory */
void platform_map_memory(void* ptr, size_t len);

/* Delay for given amount of milliseconds */
void platform_delay(int ms);

/* Initialize video subsystem */
void platform_init_video();

#endif /* __PLATFORM_H__ */
