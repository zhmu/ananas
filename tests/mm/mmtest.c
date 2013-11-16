#include <stdio.h>
#include <string.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include "test-framework.h"

int
main(int argc, char* argv[])
{
	size_t initial_mem_avail, initial_mem_total;
	addr_t tmp;

	/* Initialize the framework; it will initialize the memory code for us */
	framework_init();
	kmem_stats(&initial_mem_avail, &initial_mem_total);

	/* Allocate 2 pages; we expect them to be contiguous */
	addr_t ptr1 = (addr_t)kmalloc(PAGE_SIZE);
	addr_t ptr2 = (addr_t)kmalloc(PAGE_SIZE);
	EXPECT(ptr1 + PAGE_SIZE == ptr2);

	/* Available memory must be exactly two pages less */
	size_t mem_avail1, mem_total1;
	kmem_stats(&mem_avail1, &mem_total1);
	EXPECT(mem_total1 == initial_mem_total);
	EXPECT((mem_avail1 + (2 * PAGE_SIZE)) == initial_mem_avail);

	/* Free the first pointer and reallocate it; it should not change */
	kfree((void*)ptr1);
	tmp = (addr_t)kmalloc(PAGE_SIZE);
	EXPECT(ptr1 == tmp);

	/* Memory use must not change */
	size_t mem_avail2, mem_total2;
	kmem_stats(&mem_avail2, &mem_total2);
	EXPECT(mem_avail1 == mem_avail2);
	EXPECT(mem_total1 == mem_total2);

	/* Attempt to allocate a series of 2 and 10 pages */
	addr_t ptr3 = (addr_t)kmalloc(2 * PAGE_SIZE);
	EXPECT(ptr2 + PAGE_SIZE == ptr3);
	addr_t ptr4 = (addr_t)kmalloc(10 * PAGE_SIZE);
	EXPECT(ptr3 + (2 * PAGE_SIZE) == ptr4);

	/* Check memory usage */ 
	kmem_stats(&mem_avail1, &mem_total1);
	EXPECT(mem_avail1 + 12 * PAGE_SIZE == mem_avail2);
	EXPECT(mem_total1 == mem_total2);

	/* Free ptr3 and allocate 3 pages; they must come from after ptr4 */
	kfree((void*)ptr3);
	addr_t ptr5 = (addr_t)kmalloc(3 * PAGE_SIZE);
	EXPECT(ptr4 + (10 * PAGE_SIZE) == ptr5);

	/* Check memory usage */ 
	kmem_stats(&mem_avail2, &mem_total2);
	EXPECT(mem_avail2 + PAGE_SIZE == mem_avail1);
	EXPECT(mem_total1 == mem_total2);

	/* Allocate 2 pages; these must replace the ones by ptr3 freed before */
	tmp = (addr_t)kmalloc(2 * PAGE_SIZE);
	EXPECT(tmp == ptr3);

	/* Check memory usage */
	kmem_stats(&mem_avail1, &mem_total1);
	EXPECT(mem_avail1 + 2 * PAGE_SIZE == mem_avail2);
	EXPECT(mem_total1 == mem_total2);

	/* Now, free pointers 3 and 4... */
	kfree((void*)ptr3);
	kfree((void*)ptr4);
	
	/* Check memory usage */
	kmem_stats(&mem_avail2, &mem_total2);
	EXPECT(mem_avail1 + 12 * PAGE_SIZE == mem_avail2);
	EXPECT(mem_total1 == mem_total2);

	/* And allocate 12 pages; this should occupy the ptr3/4 region TODO Not yet ... */
	tmp = (addr_t)kmalloc(12 * PAGE_SIZE);
	EXPECT_FAILURE(tmp == ptr3);

	/* Check memory usage */
	kmem_stats(&mem_avail1, &mem_total1);
	EXPECT(mem_avail1 + 12 * PAGE_SIZE == mem_avail2);
	EXPECT(mem_total1 == mem_total2);

	/* Clean up the test framework; this will also output test results */
	framework_done();
	return 0;
}

/* vim:set ts=2 sw=2: */
