#ifndef __ANANAS_POWERPC_HAL_H__
#define __ANANAS_POWERPC_HAL_H__

#include <ananas/types.h>

/*
 * PowerPC-specific hardware abstraction layer; this is responsible for
 * handling firmware-specific actions like obtaining the memory map.
 */

/* A region describes a continuous region of memory */
struct HAL_REGION {
	addr_t reg_base;
	size_t reg_size;
};

/* Initialize memory map; returns total amount of memory in bytes */
size_t hal_init_memory();

/* Retrieve the memory map */
void hal_get_available_memory(struct HAL_REGION** region, int* num_regions);

/* Retrieve the CPU speed in Hz */
uint32_t hal_get_cpu_speed();


#endif /* __ANANAS_POWERPC_HAL_H__ */
