#include <ananas/types.h>
#include <ananas/wii/invalidate.h>
#include <ananas/lib.h>

/*
 * invalidate_for_read(p, size) ensures any parts of p in the cache are invalidated.
 */
void
invalidate_for_read(void* p, uint32_t size)
{
	/* calculate the number of 32-byte blocks to handle */
	if (size & 0x1f)
		size = (size | 0x1f) + 1;
	size >>= 5;
	KASSERT(size > 0, "invalidating nothing?");

	while(size--) {
		__asm __volatile("dcbi %%r0, %0" : : "r" (p) : "memory");
		p += 32;
	}

	__asm __volatile("sync; isync");
}

/*
 * invalidate_after_write() ensures that p is completely written to memory.
 */
void
invalidate_after_write(void* p, uint32_t size)
{
	/* calculate the number of 32-byte blocks to handle */
	if (size & 0x1f)
		size = (size | 0x1f) + 1;
	size >>= 5;
	KASSERT(size > 0, "invalidating nothing?");

	while(size--) {
		__asm __volatile("dcbst %%r0, %0" : : "r" (p) : "memory");
		p += 32;
	}

	__asm __volatile("sync; isync");
}

/* vim:set ts=2 sw=2: */
