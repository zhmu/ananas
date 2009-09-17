#include "mm.h"
#include "vm.h"
#include "lib.h"

static int zone_root_initialized;
static struct MM_ZONE* zone_root;

void
mm_init()
{
	zone_root_initialized = 0;
	zone_root = NULL;
}

void
mm_zone_add(addr_t addr, size_t length)
{
	KASSERT(addr % PAGE_SIZE == 0, "addr 0x%x isn't page-aligned", addr);
	KASSERT(length % PAGE_SIZE == 0, "length 0x%x isn't page-aligned", length);

	/*
	 * First of all, we need to calculate the number of pages needed to store the
	 * administration involved in this zone.
	 *
	 * Note that we actually allocate these structures from the end of
	 * the kernel memory, because this ensures we do not overwrite
	 * important structures etc.
	 */
	int num_pages =
		(
			/* MM_ZONE struct describing the zone itself */
			sizeof(struct MM_ZONE) +
			/* each page is described by a MM_CHUNK structure */
			(length / PAGE_SIZE) * sizeof(struct MM_CHUNK) +
			PAGE_SIZE - 1
		) / PAGE_SIZE;
	addr_t admin_addr = addr + length - (num_pages * PAGE_SIZE);
	vm_map(admin_addr, num_pages);

	/*
	 * Initialize the zone.
	 */
	struct MM_ZONE* zone = (struct MM_ZONE*)admin_addr;
	zone->address = addr;
	zone->length = length / PAGE_SIZE;
	zone->num_chunks = length / PAGE_SIZE;
	zone->num_free = zone->num_chunks - num_pages;
	zone->num_cont_free = zone->num_free;
	zone->flags = 0;
	zone->chunks = (struct MM_CHUNK*)(admin_addr + sizeof(struct MM_ZONE));
	zone->next_zone = NULL;
#if 0
kprintf("zone_add: addr=%x,len=%x,num_chunks=%x,num_free=%x,np=%x\n", addr, length, zone->num_chunks, zone->length, zone->num_free);
kprintf("zone_add: addr=%x, chunks=%x\n", zone->address, zone->chunks);
#endif

	/*
	 * Figure out the location of the first chunk's data - this is directly after
	 * the zone and chunk structures, rounded to a page.
	 */
	addr_t data_addr = addr + (num_pages * PAGE_SIZE);

	/*
	 * Initialize the chunks, one at a time.
	 */
	unsigned int n;
	for (n = 0; n < zone->num_chunks; n++, data_addr += PAGE_SIZE) {
		struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)zone->chunks + n * sizeof(struct MM_CHUNK));
		chunk->address = data_addr;
		chunk->flags = 0;
		chunk->chain_length = zone->num_chunks - n;
	}

	/* Finally, add this zone to the available zones */
	if (zone_root_initialized) {
		struct MM_ZONE* curzone;
		for (curzone = zone_root; curzone->next_zone != NULL; curzone = curzone->next_zone);
		curzone->next_zone = zone;
	} else {
		zone_root = zone; zone_root_initialized = 1;
	}
}

void
kmem_stats(size_t* avail, size_t* total)
{
	*avail = 0; *total = 0;

	if (!zone_root_initialized)
		return;

	struct MM_ZONE* curzone = zone_root;
	while (1) {
		*total += curzone->length * PAGE_SIZE;
		*avail += curzone->num_free * PAGE_SIZE;
		curzone = curzone->next_zone;
		if (curzone == NULL)
			break;
	}
}

/*
 * Allocates a chunk of memory, but does not map it.
 */
void*
kmem_alloc(size_t len)
{
	struct MM_ZONE* curzone = zone_root;

	/* round up to a full page */
	if (len % PAGE_SIZE > 0)
		len += PAGE_SIZE - (len % PAGE_SIZE);

	len /= PAGE_SIZE;
	for (curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		/* Skip any zone that hasn't got enough space */
		if (curzone->num_cont_free < len)
			continue;

		unsigned int n;
		for (n = 0; n < curzone->num_chunks; n++) {
			struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)curzone->chunks + n * sizeof(struct MM_CHUNK));

			if (chunk->flags & MM_CHUNK_FLAG_USED)
				continue;
			if (chunk->chain_length < len)
				continue;

			/*
			 * OK, we hit a chunk that we can use. 
			 */
			addr_t addr = chunk->address;
			while (len > 0) {
				chunk->flags |= MM_CHUNK_FLAG_USED;
				chunk->chain_length = len;
				chunk++; len--;
			}

			return (void*)addr;
		}
	}

	/* No zones available to honor this request */
	return NULL;
}

void*
kmalloc(size_t len)
{
	/* Round len up to a full page*/
	if (len % PAGE_SIZE > 0)
		len += PAGE_SIZE - (len % PAGE_SIZE);
	void* ptr = kmem_alloc(len);
	len /= PAGE_SIZE;
	vm_map((addr_t)ptr, len);
	return ptr;
}

void
kfree(void* addr)
{
	KASSERT((addr_t)addr % PAGE_SIZE == 0, "addr 0x%x isn't page-aligned", (addr_t)addr);

	/*
	 * This is quite unpleasant; we have to trace through all zones before we know
	 * where this address belongs. And then, we can free it.
	 */
	struct MM_ZONE* curzone;
	for (curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		unsigned int n;
		for (n = 0; n < curzone->num_chunks; n++) {
			struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)curzone->chunks + n * sizeof(struct MM_CHUNK));
			if (chunk->address != (addr_t)addr)
				continue;

			if ((chunk->flags & MM_CHUNK_FLAG_USED) == 0)
				panic("freeing unallocated pointer 0x%x", (addr_t)addr);

			/*
			 * OK, we located the chunk. Just mark it as free for now
			 * XXX we should merge it, but that's for later XXX
			 */
			unsigned int i = chunk->chain_length;
			while (i > 0) {
				KASSERT(chunk->flags & MM_CHUNK_FLAG_USED, "chunk 0x%x (%u) should be allocated, but isn't!", chunk, i);
				chunk->flags &= ~MM_CHUNK_FLAG_USED;
				chunk++; i--;
			}
			return;
		}
	}

	panic("freeing unlocatable pointer 0x%x", (addr_t)addr);
}

/* vim:set ts=2 sw=2: */
