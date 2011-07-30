#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef __nonnull
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include "test-framework.h"

#define MM_REGION_LEN (1 * 1024 * 1024)

const char* console_getbuf();

void
framework_init()
{
	/*
	 * Part 1: A single regions. Note that regions must be page-aligned.
	 */
	void* region1ptr = malloc(MM_REGION_LEN + PAGE_SIZE - 1);
	addr_t region1addr = (addr_t)region1ptr;
	if (region1addr % PAGE_SIZE > 0)
		region1addr += PAGE_SIZE - (region1addr % PAGE_SIZE);
	mm_zone_add(region1addr, MM_REGION_LEN);

	/*
	 * We should have memory available now, and never more than our initial
	 * region.
	 */
	size_t initial_mem_avail, initial_mem_total;
	kmem_stats(&initial_mem_avail, &initial_mem_total);
	assert(initial_mem_avail < initial_mem_total);
	assert(initial_mem_total <= MM_REGION_LEN);
}

void
framework_done()
{
	/* Ensure the console is empty; if not, this is a bug */
	const char* ptr = console_getbuf();
	assert(*ptr == '\0');
}

/* vim:set ts=2 sw=2: */
