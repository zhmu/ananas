#include <ananas/types.h>
#include <ananas/cdefs.h>

#ifndef __MM_H__
#define __MM_H__

#define MM_ZONE_MAGIC		0x19830708
#define MM_CHUNK_MAGIC		0x20080801

#define MM_CHUNK_FLAG_USED	0x0001
#define MM_CHUNK_FLAG_RESERVED	0x0002

/*
 * A zone describes a contiguous piece of memory. Every zone consists of a
 * number of chunks, and a chunk can either be allocated or be available.
 */
struct MM_ZONE {
	/* Zone's base address */
	addr_t	address;

	/* Magic number, to check whether the zone wasn't corrupted */
	uint32_t magic;

	/* Zone length, in pages */
	size_t	length;

	/* Total number of chunks */
	size_t num_chunks;

	/* Number of available pages */
	size_t	num_free;

	/* Number of contigous available pages */
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

	/* Magic number, to check whether the chunk wasn't corrupted */
	uint32_t magic;

	const char* file;
	int line;

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
void* kmem_alloc2(size_t len, const char* fname, int line);
void kmem_free(void* ptr);
void kmem_list();
void* kmalloc2(size_t len, const char* fname, int line) __malloc;
void  kfree(void* ptr);

#define kmem_alloc(len) kmem_alloc2((len), __FILE__, __LINE__)
#define kmalloc(len) kmalloc2((len), __FILE__, __LINE__)

void kmem_mark_used(void* addr, size_t num_pages);
void kmem_chunk_reserve(addr_t chunk_start, addr_t chunk_end, addr_t reserved_start, addr_t reserved_end, addr_t* out_start, addr_t* out_end);

#endif /* __MM_H__ */
