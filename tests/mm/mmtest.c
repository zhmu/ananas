#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE */

#define MM_REGION1_LEN (1 * 1024 * 1024)

int
main(int argc, char* argv[])
{
	size_t initial_mem_avail, initial_mem_total;
	addr_t tmp;

	mm_init();

	/*
	 * Part 1: A single regions. Note that regions must be page-aligned.
	 */
	void* region1ptr = malloc(MM_REGION1_LEN + PAGE_SIZE - 1);
	addr_t region1addr = (addr_t)region1ptr;
	if (region1addr % PAGE_SIZE > 0)
		region1addr += PAGE_SIZE - (region1addr % PAGE_SIZE);
	mm_zone_add(region1addr, MM_REGION1_LEN);

	/*
	 * We should have memory available now, and never more than our initial
	 * region.
	 */
	kmem_stats(&initial_mem_avail, &initial_mem_total);
	assert(initial_mem_avail < initial_mem_total);
	assert(initial_mem_total <= MM_REGION1_LEN);

	/* Allocate 2 pages; we expect them to be contiguous */
	addr_t ptr1 = (addr_t)kmalloc(PAGE_SIZE);
	addr_t ptr2 = (addr_t)kmalloc(PAGE_SIZE);
	assert(ptr1 + PAGE_SIZE == ptr2);

	/* Available memory must be exactly two pages less */
	size_t mem_avail1, mem_total1;
	kmem_stats(&mem_avail1, &mem_total1);
	assert(mem_total1 == initial_mem_total);
	assert((mem_avail1 + (2 * PAGE_SIZE)) == initial_mem_avail);

	/* Free the first pointer and reallocate it; it should not change */
	kfree((void*)ptr1);
	tmp = (addr_t)kmalloc(PAGE_SIZE);
	assert(ptr1 == tmp);

	/* Memory use must not change */
	size_t mem_avail2, mem_total2;
	kmem_stats(&mem_avail2, &mem_total2);
	assert(mem_avail1 == mem_avail2);
	assert(mem_total1 == mem_total2);

	/* Attempt to allocate a series of 2 and 10 pages */
	addr_t ptr3 = (addr_t)kmalloc(2 * PAGE_SIZE);
	assert(ptr2 + PAGE_SIZE == ptr3);
	addr_t ptr4 = (addr_t)kmalloc(10 * PAGE_SIZE);
	assert(ptr3 + (2 * PAGE_SIZE) == ptr4);

	/* Check memory usage */ 
	kmem_stats(&mem_avail1, &mem_total1);
	assert(mem_avail1 + 12 * PAGE_SIZE == mem_avail2);
	assert(mem_total1 == mem_total2);

	/* Free ptr3 and allocate 3 pages; they must come from after ptr4 */
	kfree((void*)ptr3);
	addr_t ptr5 = (addr_t)kmalloc(3 * PAGE_SIZE);
	assert(ptr4 + (10 * PAGE_SIZE) == ptr5);

	/* Check memory usage */ 
	kmem_stats(&mem_avail2, &mem_total2);
	assert(mem_avail2 + PAGE_SIZE == mem_avail1);
	assert(mem_total1 == mem_total2);

	/* Allocate 2 pages; these must replace the ones by ptr3 freed before */
	tmp = (addr_t)kmalloc(2 * PAGE_SIZE);
	assert(tmp == ptr3);

	/* Check memory usage */
	kmem_stats(&mem_avail1, &mem_total1);
	assert(mem_avail1 + 2 * PAGE_SIZE == mem_avail2);
	assert(mem_total1 == mem_total2);

	/* Now, free pointers 3 and 4... */
	kfree((void*)ptr3);
	kfree((void*)ptr4);
	
	/* Check memory usage */
	kmem_stats(&mem_avail2, &mem_total2);
	assert(mem_avail1 + 12 * PAGE_SIZE == mem_avail2);
	assert(mem_total1 == mem_total2);

	/* And allocate 12 pages; this should occupy the ptr3/4 region */
	tmp = (addr_t)kmalloc(12 * PAGE_SIZE);
	assert(tmp == ptr3);

	/* Check memory usage */
	kmem_stats(&mem_avail1, &mem_total1);
	assert(mem_avail1 + 12 * PAGE_SIZE == mem_avail2);
	assert(mem_total1 == mem_total2);
	return 0;
}

/* vim:set ts=2 sw=2: */
