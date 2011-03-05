#include <ananas/types.h>
#include <machine/param.h>
#include <ananas/lock.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <ananas/lib.h>

static struct MM_ZONE* zone_root;
static struct SPINLOCK spl_mm;

void kmem_dump();

static void
kmem_assert(int c, const char* cond, int line, const char* msg, ...)
{
 va_list ap;
	if (c)
		return;

//	kmem_dump();

	va_start(ap, msg);
	vaprintf(msg, ap);
	kprintf("\n");
	va_end(ap);
	panic("%s:%u: assertion '%s' failed", __FILE__, line, cond);
}

#define KMEM_ASSERT(x,msg...) kmem_assert((x), STRINGIFY(x), __LINE__, ##msg);

void
mm_init()
{
	zone_root = NULL;
	spinlock_init(&spl_mm);
}

void
mm_zone_add(addr_t addr, size_t length)
{
	KMEM_ASSERT(addr % PAGE_SIZE == 0, "addr 0x%x isn't page-aligned", addr);
	KMEM_ASSERT(length % PAGE_SIZE == 0, "length 0x%x isn't page-aligned", length);

	/* Don't bother dealing with any zone less than 2 pages */
	if (length < PAGE_SIZE * 2)
		return;

	/*
	 * First of all, we need to calculate the number of pages needed to store the
	 * administration involved in this zone.
	 *
	 * Note that we actually allocate these structures from the end of
	 * the kernel memory, because this ensures we do not overwrite
	 * important structures etc.
	 */
	size_t num_chunks = (length - PAGE_SIZE) / (PAGE_SIZE + sizeof(struct MM_CHUNK));
	size_t num_pages =
		(
			/* MM_ZONE struct describing the zone itself */
			sizeof(struct MM_ZONE) +
			/* For every chunk, we have a MM_CHUNK structure */
			num_chunks * sizeof(struct MM_CHUNK) +
			/* Round up */
			PAGE_SIZE - 1
		) / PAGE_SIZE;
	addr_t admin_addr = addr + length - (num_pages * PAGE_SIZE);
	KMEM_ASSERT(admin_addr + (num_pages * PAGE_SIZE) == (addr + length), "adminstration does not fill up zone");
	vm_map(admin_addr, num_pages);

	/*
	 * Initialize the zone.
	 */
	struct MM_ZONE* zone = (struct MM_ZONE*)admin_addr;
	zone->address = addr;
	zone->magic = MM_ZONE_MAGIC;
	zone->length = num_chunks;
	zone->num_chunks = num_chunks - num_pages;
	zone->num_free = zone->num_chunks;
	zone->num_cont_free = zone->num_free;
	zone->flags = 0;
	zone->chunks = (struct MM_CHUNK*)(admin_addr + sizeof(struct MM_ZONE));
	zone->next_zone = NULL;
#if 0
kprintf("zone_add: memaddr=%p. length=%x => #chunks=%u, #free=%u\n", addr, length, zone->num_chunks, zone->num_free);
kprintf("zone_add: chunkaddr=%x, chunks=%x\n", zone->address, zone->chunks);
#endif

	/*
	 * Figure out the location of the first chunk's data - this is directly after
	 * the zone and chunk structures, rounded to a page.
	 */
	addr_t data_addr = addr;

	/*
	 * Initialize the chunks, one at a time.
	 */
	unsigned int n;
	for (n = 0; n < zone->num_chunks; n++, data_addr += PAGE_SIZE) {
		struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)zone->chunks + n * sizeof(struct MM_CHUNK));
		KMEM_ASSERT(data_addr < admin_addr, "chunk %u@%p would overwrite administrativa", n, chunk);
		chunk->address = data_addr;
		chunk->magic = MM_CHUNK_MAGIC;
		chunk->flags = 0;
		chunk->chain_length = zone->num_chunks - n;
	}

	/* Finally, add this zone to the available zones */
	spinlock_lock(&spl_mm);
	if (zone_root != NULL) {
		struct MM_ZONE* curzone;
		for (curzone = zone_root; curzone->next_zone != NULL; curzone = curzone->next_zone);
		curzone->next_zone = zone;
	} else {
		zone_root = zone;
	}

	spinlock_unlock(&spl_mm);

	/*
	 * If we just added a zone that began at 0x0, reserve the first page.
	 */
	if (addr == 0) 
		kmem_mark_used(0, 1);
}

void
kmem_stats(size_t* avail, size_t* total)
{
	*avail = 0; *total = 0;

	spinlock_lock(&spl_mm);
	for (struct MM_ZONE* curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		*total += curzone->length * PAGE_SIZE;
		*avail += curzone->num_free * PAGE_SIZE;
	}
	spinlock_unlock(&spl_mm);
}

void
kmem_dump()
{
	kprintf("zone dump\n");
	for (struct MM_ZONE* curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		kprintf("zone 0x%p: addr 0x%x, length 0x%x, #chunks %u, num_free %u, num_cont_free %u, flags %u, chunks %p, next %p\n",
		 curzone, curzone->address, curzone->length, curzone->num_chunks,
		 curzone->num_free, curzone->num_cont_free, curzone->flags, curzone->chunks,
		 curzone->next_zone);

		struct MM_CHUNK* curchunk = curzone->chunks;
		for (unsigned int n = 0; n < curzone->num_chunks; n++) {
			kprintf("   %u: @ %x, flags %x, chain %u\n", n,
			 curchunk->address, curchunk->flags, curchunk->chain_length);
			curchunk++;
		}
	}
}

/*
 * Allocates a chunk of memory, but does not map it. Note that len is in PAGES !
 */
void*
kmem_alloc(size_t len)
{
	spinlock_lock(&spl_mm);
	for (struct MM_ZONE* curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		KMEM_ASSERT(curzone->magic == MM_ZONE_MAGIC, "zone %p corrupted", curzone);
#ifdef NOTYET
		/* Skip any zone that hasn't got enough space */
		if (curzone->num_cont_free < len)
			continue;
#endif

		for (unsigned int n = 0; n < curzone->num_chunks; n++) {
			struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)curzone->chunks + n * sizeof(struct MM_CHUNK));
			KMEM_ASSERT(chunk->magic == MM_CHUNK_MAGIC, "chunk %p corrupted", chunk);

			if (chunk->flags & MM_CHUNK_FLAG_USED)
				continue;
			if (chunk->chain_length < len)
				continue;

			/*
			 * OK, we hit a chunk that we can use. 
			 */
			curzone->num_free -= len;
			addr_t addr = chunk->address;
			while (len > 0) {
				KMEM_ASSERT(!(chunk->flags & MM_CHUNK_FLAG_USED), "chunk %p in free chain is used", chunk);
				chunk->flags |= MM_CHUNK_FLAG_USED;
				chunk->chain_length = len;
				chunk++; len--;
			}

			spinlock_unlock(&spl_mm);
			return (void*)addr;
		}

#ifdef NOTYET
		panic("zone %p is supposed to have space for %u pages, but does not", curzone, len);
#endif
	}

	/* No zones available to honor this request */
	spinlock_unlock(&spl_mm);
	return NULL;
}

void
kmem_free(void* addr)
{
	KMEM_ASSERT((addr_t)addr % PAGE_SIZE == 0, "addr 0x%x isn't page-aligned", (addr_t)addr);

	spinlock_lock(&spl_mm);
	for (struct MM_ZONE* curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		/* Skip the zone if we know the address won't be in it */
		if ((addr_t)addr < (addr_t)curzone->address ||
		    (addr_t)addr > (addr_t)(curzone->address + curzone->length * PAGE_SIZE))
			continue;

		/*
		 * We have no choice but to do a sequantial scan for the chunk in question.
		 */
		for (unsigned int n = 0; n < curzone->num_chunks; n++) {
			struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)curzone->chunks + n * sizeof(struct MM_CHUNK));
			if (chunk->address != (addr_t)addr)
				continue;

			/*
			 * OK, we located the chunk. Just mark it as free for now
			 * XXX we should merge it, but that's for later XXX
			 */
			unsigned int i = chunk->chain_length;
			curzone->num_free += i;
			while (i > 0) {
				KMEM_ASSERT(chunk->flags & MM_CHUNK_FLAG_USED, "chunk 0x%x (%u) should be allocated, but isn't!", chunk, i);
				KMEM_ASSERT((chunk->flags & MM_CHUNK_FLAG_RESERVED) == 0, "chunk 0x%x (%u) is forced as allocated", chunk, i);
				KMEM_ASSERT(chunk->chain_length == i, "chunk %p does not belong in chain", chunk);
				chunk->flags &= ~MM_CHUNK_FLAG_USED;
				chunk++; i--;
			}
			spinlock_unlock(&spl_mm);
			return;
		}
	}

	spinlock_unlock(&spl_mm);
	KMEM_ASSERT(0, "freeing unlocatable pointer 0x%x", (addr_t)addr);
}

void*
kmalloc(size_t len)
{
	/* Calculate len in pages - always round up */
	if (len & (PAGE_SIZE - 1))
		len += PAGE_SIZE;
	len /= PAGE_SIZE;

	void* ptr = kmem_alloc(len);
	if (ptr == NULL) {
		kmem_dump();
		panic("kmalloc: out of memory");
	}
	vm_map((addr_t)ptr, len);
	return ptr;
}

void
kfree(void* addr)
{
	kmem_free(addr);
}

void
kmem_mark_used(void* addr, size_t num_pages)
{
	KMEM_ASSERT((addr_t)addr % PAGE_SIZE == 0, "addr 0x%x isn't page-aligned", (addr_t)addr);

	size_t num_marked = 0;

	spinlock_lock(&spl_mm);
	for (struct MM_ZONE* curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		/* Skip the zone if we know the address won't be in it */
		if ((addr_t)addr < (addr_t)curzone->address ||
		    (addr_t)addr + num_pages > (addr_t)(curzone->address + curzone->length * PAGE_SIZE))
			continue;

		/*
		 * First of all, locate our chunk of memory. As we need to walk through the
		 * zone from front to back, we locate the final available free item as well,
		 * as we must update it to exclude the memory we're marking as available.
		 */
		unsigned int n = 0;
		int last_free_chunk = -1;
		for (; n < curzone->num_chunks; n++) {
			struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)curzone->chunks + n * sizeof(struct MM_CHUNK));

			if (chunk->address == (addr_t)addr)
				break;

			if ((chunk->flags & MM_CHUNK_FLAG_USED) == 0) {
				if (last_free_chunk < 0)
					last_free_chunk = n;
			} else {
				/* Chunk is used, so we may reset the counter */
				last_free_chunk = -1;
			}
		}
		KMEM_ASSERT(curzone->num_chunks != n, "chunk not found");

		/*
		 * OK, memory found. We now have to shrink the memory chunk before us,
		 * as these pages may still consider us free.
		 */
		if (last_free_chunk >= 0) {
			for (int i = last_free_chunk; i < n; i++) {
				struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)curzone->chunks + i * sizeof(struct MM_CHUNK));
				KMEM_ASSERT((chunk->flags & MM_CHUNK_FLAG_USED) == 0, "attempt to mark chunk that is currently in use");
				chunk->chain_length = n - i;
			}
		}

		for (; n < curzone->num_chunks && num_marked < num_pages; n++) {
			struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)curzone->chunks + n * sizeof(struct MM_CHUNK));

			if (chunk->address != (addr_t)addr)
				continue;

			chunk->flags |= MM_CHUNK_FLAG_USED | MM_CHUNK_FLAG_RESERVED;
			chunk->chain_length = num_pages - num_marked;

			num_marked++; addr += PAGE_SIZE;
		}
		curzone->num_free -= num_marked;
	}
	spinlock_unlock(&spl_mm);

	KMEM_ASSERT(num_marked == num_pages, "could only mark %u of %u pages as used", num_marked, num_pages);
}

/* vim:set ts=2 sw=2: */
