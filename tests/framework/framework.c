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
static int test_passed = 0;
static int test_failed = 0;
static int test_todo = 0;
static int test_total = 0;

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
	EXPECT(initial_mem_avail < initial_mem_total);
	EXPECT(initial_mem_total <= MM_REGION_LEN);
}

void
framework_done()
{
	/* Ensure the console is empty; if not, this is a bug */
	const char* ptr = console_getbuf();
	EXPECT(*ptr == '\0');

	printf("test summary: %u test(s), %u passed, %u failed (%u todo)\n",
	 test_total, test_passed, test_failed, test_todo);
}

void
framework_expect(int cond, enum FRAMEWORK_EXPECTING e, const char* file, int line, const char* expr)
{
	switch (e) {
		default:
		case SUCCESS:
			printf("%s %s:%u %s\n", cond ? "ok" : "fail", file, line, expr);
			(cond) ? test_passed++ : test_failed++;
			break;
		case FAILURE:
			printf("%s %s:%u %s\n", cond ? "ok(unexpected)" : "fail(expected)", file, line, expr);
			test_failed++;
			test_todo++;
			break;
	}
	test_total++;
}

/* vim:set ts=2 sw=2: */
