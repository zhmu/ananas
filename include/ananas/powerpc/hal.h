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

/* Initialize interrupt handling */
void hal_init_interrupts();

/* Called when an external interrupt occurs */
void hal_external_interrupt();

/* Late initialization; performed right before the console */
void hal_init_late();

/* Post MMU initialization; called right after the MMU is live and memory has been added */
void hal_init_post_mmu();

#endif /* __ANANAS_POWERPC_HAL_H__ */
