#ifndef __ANANAS_PAGE_H__
#define __ANANAS_PAGE_H__

#include <ananas/dqueue.h>
#include <ananas/lock.h>

#define PAGE_NUM_ORDERS 10

struct PAGE {
	DQUEUE_FIELDS(struct PAGE);

	/* Mapped address, if any */
	addr_t p_addr;

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

/* Allocates 2^order pages and maps it to kernel memory using vm_flags */
void* page_alloc_order_mapped(int order, struct PAGE** p, int vm_flags);

/* Allocates a single mapped page */
inline static void* page_alloc_single_mapped(struct PAGE** p, int vm_flags) {
	return page_alloc_order_mapped(0, p, vm_flags);
}

/* Allocates enough pages to hold length bytes */
struct PAGE* page_alloc_length(size_t length);

/* Allocates enough pages to hold length bytes and maps it to kernel memory */
void* page_alloc_length_mapped(size_t length, struct PAGE** p, int vm_flags);

/* Retrieve the page statistics */
void page_get_stats(unsigned int* total_pages, unsigned int* avail_pages);

#endif /* __ANANAS_PAGE_H__ */
