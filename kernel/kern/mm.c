#include <ananas/types.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <ananas/lock.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <ananas/lib.h>

#ifdef TESTFRAMEWORK
/*
 * XXXTEST This is a gross hack - we need to refrain from using the virtual/physical
 * kernel memory structure when we are using the test framework because it can only
 * use the OS malloc to obtain a piece and thus will have that P=V. By redefining
 * the macro to a no-op, things will actually work - it's a bit shameful that we
 * need to do it here...
 */
#undef KVTOP
#define KVTOP(x) (x)
#endif

static struct MM_ZONE* zone_root = NULL;
static spinlock_t spl_mm = SPINLOCK_DEFAULT_INIT;

void
mm_init()
{
	spinlock_init(&spl_mm);
}

void
mm_zone_add(addr_t addr, size_t length)
{
	KASSERT(addr % PAGE_SIZE == 0, "addr 0x%x isn't page-aligned", addr);
	KASSERT(length % PAGE_SIZE == 0, "length 0x%x isn't page-aligned", length);

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
	KASSERT(admin_addr + (num_pages * PAGE_SIZE) == (addr + length), "adminstration does not fill up zone");
	struct MM_ZONE* zone = vm_map_kernel(admin_addr, num_pages, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_KERNEL);

	/*
	 * Initialize the zone.
	 */
	zone->address = addr;
	zone->magic = MM_ZONE_MAGIC;
	zone->length = num_chunks;
	zone->num_chunks = num_chunks - num_pages;
	zone->num_free = zone->num_chunks;
	zone->num_cont_free = zone->num_free;
	zone->flags = 0;
	zone->chunks = (struct MM_CHUNK*)((addr_t)zone + sizeof(struct MM_ZONE));
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
		KASSERT(data_addr < admin_addr, "chunk %u@%p would overwrite administrativa", n, chunk);
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

#if 0
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
		for (unsigned int n = 0; n < curzone->num_chunks; n++, curchunk++) {
			if ((curchunk->flags & MM_CHUNK_FLAG_USED) == 0)
				continue;
			kprintf("   %u: @ %x, %s:%u, flags %x, chain %u\n", n,
			 curchunk->address,
			 curchunk->file, curchunk->line,
	 		 curchunk->flags, curchunk->chain_length);
		}
	}
}
#endif

void
kmem_list()
{
	kprintf("memory list\n");
	for (struct MM_ZONE* curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		kprintf("zone 0x%p: addr 0x%x, length 0x%x, #chunks %u, num_free %u, num_cont_free %u, flags %u, chunks %p, next %p\n",
		 curzone, curzone->address, curzone->length, curzone->num_chunks,
		 curzone->num_free, curzone->num_cont_free, curzone->flags, curzone->chunks,
		 curzone->next_zone);

		struct MM_CHUNK* curchunk = curzone->chunks;
		for (unsigned int n = 0; n < curzone->num_chunks; /* nothing */) {
			if ((curchunk->flags & MM_CHUNK_FLAG_USED) == 0)
				continue;
			kprintf("   %u: @ %x, %s:%u, %u KB\n", n,
			 curchunk->address,
			 curchunk->file, curchunk->line,
	 		 curchunk->chain_length * PAGE_SIZE / 1024);
			n += curchunk->chain_length;
			curchunk += curchunk->chain_length;
		}
	}
}


/*
 * Allocates a chunk of memory, but does not map it. Note that len is in PAGES !
 */
void*
kmem_alloc2(size_t len, const char* file, int line)
{
	spinlock_lock(&spl_mm);
	for (struct MM_ZONE* curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		KASSERT(curzone->magic == MM_ZONE_MAGIC, "zone %p corrupted", curzone);
#ifdef NOTYET
		/* Skip any zone that hasn't got enough space */
		if (curzone->num_cont_free < len)
			continue;
#endif

		for (unsigned int n = 0; n < curzone->num_chunks; n++) {
			struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)curzone->chunks + n * sizeof(struct MM_CHUNK));
			KASSERT(chunk->magic == MM_CHUNK_MAGIC, "chunk %p corrupted", chunk);

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
				KASSERT(!(chunk->flags & MM_CHUNK_FLAG_USED), "chunk %p in free chain is used", chunk);
				chunk->flags |= MM_CHUNK_FLAG_USED;
				chunk->chain_length = len;
				chunk->file = file;
				chunk->line = line;
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
	KASSERT((addr_t)addr % PAGE_SIZE == 0, "addr 0x%x isn't page-aligned", (addr_t)addr);

	spinlock_lock(&spl_mm);
	for (struct MM_ZONE* curzone = zone_root; curzone != NULL; curzone = curzone->next_zone) {
		/* Skip the zone if we know the address won't be in it */
		if ((addr_t)addr < (addr_t)curzone->address ||
		    (addr_t)addr > (addr_t)(curzone->address + curzone->length * PAGE_SIZE))
			continue;

		/*
		 * We have no choice but to do a sequantial scan for the chunk in question.
		 */
		struct MM_CHUNK* chunk = (struct MM_CHUNK*)curzone->chunks;
		for (unsigned int n = 0; n < curzone->num_chunks; n++, chunk++) {
			if (chunk->address != (addr_t)addr)
				continue;

			/*
			 * OK, we located the chunk. Just mark it as free for now
			 * XXX we should merge it, but that's for later XXX
			 */
			unsigned int i = chunk->chain_length;
			curzone->num_free += i;
			while (i > 0) {
				KASSERT(chunk->flags & MM_CHUNK_FLAG_USED, "chunk 0x%x (%u) should be allocated, but isn't!", chunk, i);
				KASSERT((chunk->flags & MM_CHUNK_FLAG_RESERVED) == 0, "chunk 0x%x (%u) is forced as allocated", chunk, i);
				KASSERT(chunk->chain_length == i, "chunk %p does not belong in chain", chunk);
				chunk->flags &= ~MM_CHUNK_FLAG_USED;
				chunk++; i--;
			}
			spinlock_unlock(&spl_mm);
			return;
		}
	}

	spinlock_unlock(&spl_mm);
	panic("freeing unlocatable pointer 0x%x", (addr_t)addr);
}

void*
kmalloc2(size_t len, const char* file, int line)
{
	/* Calculate len in pages - always round up */
	if (len & (PAGE_SIZE - 1))
		len += PAGE_SIZE;
	len /= PAGE_SIZE;

	void* ptr = kmem_alloc2(len, file, line);
	if (ptr == NULL) {
		panic("kmalloc: out of memory");
	}
	return vm_map_kernel((addr_t)ptr, len, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_KERNEL);
}

void
kfree(void* addr)
{
	kmem_free((void*)KVTOP((addr_t)addr));
}

void
kmem_mark_used(void* addr, size_t num_pages)
{
	KASSERT((addr_t)addr % PAGE_SIZE == 0, "addr 0x%x isn't page-aligned", (addr_t)addr);

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
		KASSERT(curzone->num_chunks != n, "chunk not found");

		/*
		 * OK, memory found. We now have to shrink the memory chunk before us,
		 * as these pages may still consider us free.
		 */
		if (last_free_chunk >= 0) {
			for (int i = last_free_chunk; i < n; i++) {
				struct MM_CHUNK* chunk = (struct MM_CHUNK*)((addr_t)curzone->chunks + i * sizeof(struct MM_CHUNK));
				KASSERT((chunk->flags & MM_CHUNK_FLAG_USED) == 0, "attempt to mark chunk that is currently in use");
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

	KASSERT(num_marked == num_pages, "could only mark %u of %u pages as used", num_marked, num_pages);
}

/* vim:set ts=2 sw=2: */
