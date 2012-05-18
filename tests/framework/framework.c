#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef __nonnull
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/init.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include "test-framework.h"

#define MM_REGION_LEN (1 * 1024 * 1024)

const char* console_getbuf();
static int test_passed = 0;
static int test_failed = 0;
static int test_todo = 0;
static int test_total = 0;

extern void* __initfuncs_begin;
extern void* __initfuncs_end;

static void
initfuncs_run()
{
	/* Create a copy the init functions so that we can sort it */
	size_t init_static_func_len  = (addr_t)&__initfuncs_end - (addr_t)&__initfuncs_begin;
	struct INIT_FUNC** ifn_chain = malloc(init_static_func_len * sizeof(struct INIT_FUNC*));
	memcpy(ifn_chain, (void*)&__initfuncs_begin, init_static_func_len);
	
	/* Sort the init functions chain; we use a simple bubble sort to do so */
	int num_init_funcs = init_static_func_len / sizeof(struct INIT_FUNC*);
	for (int i = 0; i < num_init_funcs; i++)
		for (int j = num_init_funcs - 1; j > i; j--) {
			struct INIT_FUNC** a = &ifn_chain[j];
			struct INIT_FUNC** b = &ifn_chain[j - 1];
			if (((*a)->if_subsystem > (*b)->if_subsystem) ||
			    ((*a)->if_subsystem >= (*b)->if_subsystem &&
			     (*a)->if_order >= (*b)->if_order))
				continue;
			struct INIT_FUNC* swap = *a;
			*a = *b;
			*b = swap;
		}

	/* Execute all init functions in order */
	struct INIT_FUNC** ifn = (struct INIT_FUNC**)ifn_chain;
	for (int i = 0; i < num_init_funcs; i++, ifn++)
		(*ifn)->if_func();

	/* Free our init function chain */
	free(ifn_chain);
}

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

	/* Give the initialization functions a go */
	initfuncs_run();
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
