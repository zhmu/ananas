#include "types.h"

#ifndef __MM_H__
#define __MM_H__

#define MM_CHUNK_FLAG_USED	0x0001

/*
 * A zone describes a contiguous piece of memory. Every zone consists of a
 * number of chunks, and a chunk can either be allocated or be available.
 */
struct MM_ZONE {
	/* Zone's base address */
	addr_t	address;

	/* Zone length, in pages */
	size_t	length;

	/* Total number of chunks */
	size_t num_chunks;

	/* Number of available pages */
	size_t	num_free;

	/*
	 * Maximum number of available contiguous pages. This is needed because
	 * even though a zone may have a large amount of memory available, we
	 * need to know before-hand whether the zone needs to be considered at
	 * all.
	 */
	size_t	num_cont_free;

	/* Flags */
	int	flags;

	/* Pointer to the first chunk */
	struct MM_CHUNK* chunks;

	/* Pointer to the next zone, if any */
	struct MM_ZONE* next_zone;
};

/*
 *  A chunk 
 */
struct MM_CHUNK {
	/* Chunk's base address */
	addr_t	address;

	/* Chunk flags */
	int	flags;

	/*
	 * The chain length is used to determine the number of next adjacent
	 * chunks after us; this is needed when we are freeing a chunk, since
	 * we need to free anything in the chain too.
	 */
	int	chain_length;
}; 

void mm_init();
void mm_zone_add(addr_t addr, size_t length);
void kmem_stats(size_t* avail, size_t* total);
void* kmem_alloc(size_t len);
void* kmalloc(size_t len);
void  kfree(void* ptr);

#endif /* __MM_H__ */
