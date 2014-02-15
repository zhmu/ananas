#ifndef __ANANAS_PAGE_H__
#define __ANANAS_PAGE_H__

#include <ananas/dqueue.h>
#include <ananas/lock.h>

#define PAGE_NUM_ORDERS 10

struct PAGE {
	DQUEUE_FIELDS(struct PAGE);

	/* Index within the zone */
	unsigned int p_index;

	/* Page order */
	unsigned int p_order;

	/* Owning zone */
	struct PAGE_ZONE* p_zone;
};


DQUEUE_DEFINE(page_list, struct PAGE);

struct PAGE_ZONE {
	DQUEUE_FIELDS(struct PAGE_ZONE);

	/* Lock protecting the zone */
	spinlock_t z_lock;

	/* Free pages within this zone */
	struct page_list z_free[PAGE_NUM_ORDERS];

	/* Total number of pages */
	unsigned int z_num_pages;

	/* Available number of pages */
	unsigned int z_avail_pages;

	/* First page address */
	struct PAGE* z_base;

	/* First backing memory address */
	addr_t z_phys_addr;

	/* Map with the used bitmap */
	char* z_bitmap;
};

DQUEUE_DEFINE(zone_list, struct PAGE_ZONE);

/* Add a chunk of memory to use for page allocation */
void page_zone_add(addr_t base, size_t length);

/* Allocates a block of 2^order pages */
struct PAGE* page_alloc_order(int order);

/* Allocates a single page */
inline static struct PAGE* page_alloc_single() {
	return page_alloc_order(0);
}
void page_free(struct PAGE* p);

/* Retrieves the physical address of page p */
addr_t page_get_paddr(struct PAGE* p);

#endif /* __ANANAS_PAGE_H__ */
