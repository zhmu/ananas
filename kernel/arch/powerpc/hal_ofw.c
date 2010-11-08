#include <ananas/types.h>
#include <machine/hal.h>
#include <ofw.h>

#define MAX_REGIONS 32

static struct HAL_REGION hal_avail_region[MAX_REGIONS];
static int hal_num_avail_regions = 0;

size_t
hal_init_memory()
{
	struct ofw_reg ofw_region[MAX_REGIONS];

	/* Fetch the memory phandle; we need this to obtain our memory map */
	ofw_cell_t i_memory;
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_getprop(chosen, "memory", &i_memory, sizeof(i_memory));
	ofw_cell_t p_memory = ofw_instance_to_package(i_memory);

	/*
	 * Figure out the total amount of memory we have; this is needed to determine
	 * the size of our pagetables.
	 */
	size_t memory_total = 0;
	unsigned int totallen = ofw_getprop(p_memory, "reg", ofw_region, sizeof(struct ofw_reg) * MAX_REGIONS);
	for (unsigned int i = 0; i < totallen / sizeof(struct ofw_reg); i++) {
	  memory_total += ofw_region[i].size;
	}

	/* Grab the memory map and convert it to standard form */
	unsigned int availlen = ofw_getprop(p_memory, "available", ofw_region, sizeof(struct ofw_reg) * MAX_REGIONS);
	for (unsigned int i = 0; i < availlen / sizeof(struct ofw_reg); i++) {
		hal_avail_region[hal_num_avail_regions].reg_base = ofw_region[i].base;
		hal_avail_region[hal_num_avail_regions].reg_size = ofw_region[i].size;
		hal_num_avail_regions++;
	}

	return memory_total;
}

void
hal_get_available_memory(struct HAL_REGION** region, int* num_regions)
{
	*region = hal_avail_region;
	*num_regions = hal_num_avail_regions;
}

uint32_t
hal_get_cpu_speed()
{
	/* Obtain the running CPU */
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_cell_t ihandle_cpu;
	ofw_getprop(chosen, "cpu", &ihandle_cpu, sizeof(ihandle_cpu));

	ofw_cell_t node = ofw_instance_to_package(ihandle_cpu);
	uint32_t cpu_freq = 100000000; /* XXX default to 100MHz for psim */
	if (node != -1)
		ofw_getprop(node, "clock-frequency", &cpu_freq, sizeof(cpu_freq));

	return cpu_freq;
}

/* vim:set ts=2 sw=2: */
