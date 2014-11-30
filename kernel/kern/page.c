#include <ananas/page.h>
#include <machine/param.h>
#include <ananas/lib.h>
#include <ananas/vm.h>
#include <ananas/kmem.h>
#include <ananas/dqueue.h>

#undef PAGE_DEBUG

#ifdef PAGE_DEBUG
# define DPRINTF(fmt,...) kprintf(fmt, __VA_ARGS__)
#else
# define DPRINTF(...)
#endif

static struct zone_list zones;

static inline int
get_bit(const char* map, int bit)
{
	return (map[bit / 8] & (1 << (bit & 7))) != 0;
}

static inline void
set_bit(char* map, int bit)
{
	map[bit / 8] |= 1 << (bit & 7);
}

static inline void
clear_bit(char* map, int bit)
{
	map[bit / 8] &= ~(1 << (bit & 7));
}

/* This just returns 2^order */
static inline unsigned int
order2pages(int order)
{
	unsigned int result = 1;
	for (int n = 0; n < order; n++)
		result <<= 1;
	return result;
}

void
page_dump(struct PAGE_ZONE* z)
{
	kprintf("page_dump: zone=%p total=%u avail=%u\n", z, z->z_num_pages, z->z_avail_pages);
	for (unsigned int order = 0; order < PAGE_NUM_ORDERS; order++) {
		kprintf(" order %u:", order);
		if (DQUEUE_EMPTY(&z->z_free[order])) {
			kprintf(" <empty>");
		} else {
			DQUEUE_FOREACH(&z->z_free[order], f, struct PAGE) {
				kprintf(" %p", f);
			}
		}
		kprintf("\n");
	}

#if 0
	kprintf("free pages in bitmap:");
	for (int n = 0; n < z->z_num_pages; n++)
		if (!get_bit(z->z_bitmap, n))
			kprintf(" %u", n);
	kprintf("\n");
#endif
}

void
page_free_index(struct PAGE_ZONE* z, unsigned int order, unsigned int index)
{
	struct PAGE* p = &z->z_base[index];
	DPRINTF("page_free_index(): order=%u index=%u -> p=%p\n", order, index, p);

	spinlock_lock(&z->z_lock);

	/* Clear the current index; it is available */
	clear_bit(z->z_bitmap, index);
	z->z_avail_pages += 1 << order;

	/* Add this buddy to the freelist */
	DQUEUE_ADD_TAIL(&z->z_free[order], p);

	/* Now, attempt to merge the available pages */
	while (order < PAGE_NUM_ORDERS - 1) {
		unsigned int buddy_index = index ^ (1 << order);
		if (buddy_index >= z->z_num_pages || get_bit(z->z_bitmap, buddy_index))
			break; /* not free, bail out */

		if (z->z_base[buddy_index].p_order != order)
			break; /* buddy is of different order than this block, can't merge */

		/* Clear the lower bits from the index value */
		DPRINTF("page_free_index(): order=%u, index=%u, buddy free %u, combining\n", order, index, buddy_index);
		
		/*
		 * Now, we should combine the two buddies to one; first of all, remove them
		 * both.
		 */
		DQUEUE_REMOVE(&z->z_free[order], &z->z_base[index]);
		DQUEUE_REMOVE(&z->z_free[order], &z->z_base[buddy_index]);

		/* And add a single entry to the freelist one order above us */
		order++;
		index &= ~((1 << order) - 1);
		DQUEUE_ADD_TAIL(&z->z_free[order], &z->z_base[index]);
		z->z_base[index].p_order = order;
	}

	spinlock_unlock(&z->z_lock);
}

void
page_free(struct PAGE* p)
{
	struct PAGE_ZONE* z = p->p_zone;
	page_free_index(z, p->p_order, p - z->z_base);
}

struct PAGE*
page_alloc_zone(struct PAGE_ZONE* z, unsigned int order)
{
	DPRINTF("page_alloc_zone(): z=%p, order=%u\n", z, order);

	spinlock_lock(&z->z_lock);

	/* First step is to figure out the initial order we need to use */
	unsigned int alloc_order = order;
	while (alloc_order < PAGE_NUM_ORDERS && DQUEUE_EMPTY(&z->z_free[alloc_order]))
		alloc_order++; /* nothing free here */
	DPRINTF("page_alloc_zone(): z=%p, order=%u -> alloc_order=%u\n", z, order, alloc_order);
	if (alloc_order == PAGE_NUM_ORDERS) {
		spinlock_unlock(&z->z_lock);
		return NULL;
	}

	/* Now we need to keep splitting each block from alloc_order .. order */
	for (unsigned int n = alloc_order; n >= order; n--) {
		DPRINTF("page_alloc_zone(): loop, n=%u\n", n);
		KASSERT(!DQUEUE_EMPTY(&z->z_free[n]), "freelist of order %u can't be empty", n);

		/* Grab the first block we see */
		struct PAGE* p = DQUEUE_HEAD(&z->z_free[n]);
		DQUEUE_POP_HEAD(&z->z_free[n]);

		/* And allocate it in the bitmap */
		unsigned int index = p - z->z_base;

		/* If we reached the order we wanted, we are done */ 
		if (n == order) {
			/*
			 * We only need to set the current bitmap bit; order is already correct
			 * since splitting blocks sets it for us.
			 */
			KASSERT(p->p_order == order, "wrong order?");
			set_bit(z->z_bitmap, index);
			DPRINTF("page_alloc_zone(): got page=%p, index %u\n", p, index);
			z->z_avail_pages -= 1 << order;
			spinlock_unlock(&z->z_lock);
			return p;
		}

		/* Split this block - no need to set bits because they were already free */
		unsigned int buddy_index = index ^ (1 << (n - 1));
		DPRINTF("page_alloc_zone(): n=%u, splitting index %u -> %u, %u\n", n, index, index, buddy_index);
		DPRINTF("split page0=%p, page1=%p\n", &z->z_base[index], &z->z_base[buddy_index]);
		DQUEUE_ADD_TAIL(&z->z_free[n - 1], &z->z_base[index]);
		DQUEUE_ADD_TAIL(&z->z_free[n - 1], &z->z_base[buddy_index]);
		z->z_base[index].p_order = n - 1;
		z->z_base[buddy_index].p_order = n - 1;
	}

	/* NOTREACHED */
	panic("NOTREACHED");
	return NULL;
}

void
page_zone_add(addr_t base, size_t length)
{
	/*
	 * We'll need the following:
	 *
	 * - struct PAGE_ZONE with information regarding this zone
	 * - [num_pages] bits, to see whether a page is used
	 * - [num_pages] x (struct PAGE) to contain information for a given memory page
	 *
	 * 
	 */
	unsigned int num_pages = length / PAGE_SIZE;
	unsigned int bitmap_size = (num_pages + 7) / 8;
	unsigned int num_admin_pages = (sizeof(struct PAGE_ZONE) + bitmap_size + (num_pages * sizeof(struct PAGE)) + PAGE_SIZE - 1) / PAGE_SIZE;
	DPRINTF("%s: base=%p length=%u -> num_pages=%u, num_admin_pages=%u\n", __func__, base, length, num_pages, num_admin_pages);

	char* mem = kmem_map(base, num_admin_pages * PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);

	/* Initialize the page zone; initially, we'll just mark everything as allocated */	
	struct PAGE_ZONE* z = (struct PAGE_ZONE*)mem;
	spinlock_init(&z->z_lock);
	z->z_bitmap = mem + sizeof(*z);
	for (int n = 0; n < PAGE_NUM_ORDERS; n++)
		DQUEUE_INIT(&z->z_free[n]);
	memset(z->z_bitmap, 0xff, bitmap_size);
	z->z_base = (struct PAGE*)(mem + bitmap_size + sizeof(*z));
	z->z_num_pages = num_pages - num_admin_pages;
	z->z_avail_pages = 0;
	z->z_phys_addr = base + num_admin_pages * PAGE_SIZE;

	/* Create the page structures; we mark everything as a order 0 page */
	struct PAGE* p = z->z_base;
	for (unsigned int n = 0; n < z->z_num_pages; n++, p++) {
		p->p_zone = z;
		p->p_order = 0;
	}

	/*
	 * Now, free all chunks of memory. This is slow, we could do better but for
	 * now it'll help guarantee that the implementation is correct.
	 */
	for (int n = 0; n < z->z_num_pages; n++)
		page_free_index(z, 0, n);

	/* Add the zone to the list XXX there should be some lock on zones */
	DQUEUE_ADD_TAIL(&zones, z);
}

addr_t
page_get_paddr(struct PAGE* p)
{
	struct PAGE_ZONE* z = p->p_zone;
	unsigned int index = p - z->z_base;
	return z->z_phys_addr + index * PAGE_SIZE;
}

struct PAGE*
page_alloc_order(int order)
{
	/* XXX this function has no lock on zones */

	KASSERT(order >= 0 && order < PAGE_NUM_ORDERS, "order %d out of range", order);
	KASSERT(!DQUEUE_EMPTY(&zones), "no zones");

	DQUEUE_FOREACH(&zones, z, struct PAGE_ZONE) {
		struct PAGE* page = page_alloc_zone(z, order);
		if (page != NULL)
			return page;
	}

	panic("page_alloc(): failed for order %d", order);
}

void*
page_alloc_order_mapped(int order, struct PAGE** p, int vm_flags)
{
	*p = page_alloc_order(order);
	if (*p == NULL)
		return NULL;
	return kmem_map(page_get_paddr(*p), PAGE_SIZE << order, vm_flags);
}

void*
page_alloc_length_mapped(size_t length, struct PAGE** p, int vm_flags)
{
	/* Convert length from bytes to pages */
	unsigned int pages = length / PAGE_SIZE;

	/* Now find the 2_log of this; this is the order we'll be allocating in */
	int order = 0;
	for (unsigned int n = pages; n > 1; n >>= 1)
		order++;
	if (PAGE_SIZE << order < length)
		order++;

	return page_alloc_order_mapped(order, p, vm_flags);
}

void
page_debug()
{
	DQUEUE_FOREACH(&zones, z, struct PAGE_ZONE) {
		page_dump(z);
	}
}

/* vim:set ts=2 sw=2: */
