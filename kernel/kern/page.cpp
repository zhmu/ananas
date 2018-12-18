#include <machine/param.h>
#include "kernel/kdb.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/page.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "options.h"

#undef PAGE_DEBUG

#ifdef PAGE_DEBUG
#define DPRINTF(fmt, ...) kprintf(fmt, __VA_ARGS__)
#else
#define DPRINTF(...)
#endif

static PageZoneList zones;

void Page::AssertSane() const
{
    KASSERT(p_order >= 0 && p_order < PAGE_NUM_ORDERS, "corrupt page %p", this);
}

static inline int get_bit(const char* map, int bit)
{
    return (map[bit / 8] & (1 << (bit & 7))) != 0;
}

static inline void set_bit(char* map, int bit) { map[bit / 8] |= 1 << (bit & 7); }

static inline void clear_bit(char* map, int bit) { map[bit / 8] &= ~(1 << (bit & 7)); }

static inline unsigned int bytes2order(size_t length)
{
    /* Convert length from bytes to pages */
    unsigned int pages = length / PAGE_SIZE;

    /* Now find the 2_log of this; this is the order we'll be allocating in */
    int order = 0;
    for (unsigned int n = pages; n > 1; n >>= 1)
        order++;
    if (PAGE_SIZE << order < length)
        order++;

    return order;
}

void page_free_index(PageZone& z, unsigned int order, unsigned int index)
{
    Page& p = z.z_base[index];
    DPRINTF("page_free_index(): order=%u index=%u -> p=%p\n", order, index, &p);

    SpinlockGuard g(z.z_lock);

    /* Clear the current index; it is available */
    clear_bit(z.z_bitmap, index);
    z.z_avail_pages += 1 << order;

    /* Add this buddy to the freelist */
    z.z_free[order].push_back(p);

    /* Now, attempt to merge the available pages */
    while (order < PAGE_NUM_ORDERS - 1) {
        unsigned int buddy_index = index ^ (1 << order);
        if (buddy_index >= z.z_num_pages || get_bit(z.z_bitmap, buddy_index))
            break; /* not free, bail out */

        if (z.z_base[buddy_index].p_order != order)
            break; /* buddy is of different order than this block, can't merge */

        /* Clear the lower bits from the index value */
        DPRINTF(
            "page_free_index(): order=%u, index=%u, buddy free %u, combining\n", order, index,
            buddy_index);

        /*
         * Now, we should combine the two buddies to one; first of all, remove them
         * both.
         */
        z.z_free[order].remove(z.z_base[index]);
        z.z_free[order].remove(z.z_base[buddy_index]);

        /* And add a single entry to the freelist one order above us */
        order++;
        index &= ~((1 << order) - 1);
        z.z_free[order].push_back(z.z_base[index]);
        z.z_base[index].p_order = order;
    }
}

void page_free(Page& p)
{
    p.AssertSane();

    PageZone& z = *p.p_zone;
    page_free_index(z, p.p_order, &p - z.z_base);
}

Page* page_alloc_zone(PageZone& z, unsigned int order)
{
    DPRINTF("page_alloc_zone(): z=%p, order=%u\n", &z, order);

    SpinlockGuard g(z.z_lock);

    /* First step is to figure out the initial order we need to use */
    unsigned int alloc_order = order;
    while (alloc_order < PAGE_NUM_ORDERS && z.z_free[alloc_order].empty())
        alloc_order++; /* nothing free here */
    DPRINTF("page_alloc_zone(): z=%p, order=%u -> alloc_order=%u\n", &z, order, alloc_order);
    if (alloc_order == PAGE_NUM_ORDERS)
        return nullptr;

    /* Now we need to keep splitting each block from alloc_order .. order */
    for (unsigned int n = alloc_order; n >= order; n--) {
        DPRINTF("page_alloc_zone(): loop, n=%u\n", n);
        KASSERT(!z.z_free[n].empty(), "freelist of order %u can't be empty", n);

        /* Grab the first block we see */
        Page& p = z.z_free[n].front();
        z.z_free[n].pop_front();

        /* And allocate it in the bitmap */
        unsigned int index = &p - z.z_base;

        /* If we reached the order we wanted, we are done */
        if (n == order) {
            /*
             * We only need to set the current bitmap bit; order is already correct
             * since splitting blocks sets it for us.
             */
            KASSERT(p.p_order == order, "wrong order?");
            set_bit(z.z_bitmap, index);
            DPRINTF("page_alloc_zone(): got page=%p, index %u\n", &p, index);
            z.z_avail_pages -= 1 << order;
            return &p;
        }

        /* Split this block - no need to set bits because they were already free */
        unsigned int buddy_index = index ^ (1 << (n - 1));
        DPRINTF(
            "page_alloc_zone(): n=%u, splitting index %u -> %u, %u\n", n, index, index,
            buddy_index);
        DPRINTF("split page0=%p, page1=%p\n", &z->z_base[index], &z->z_base[buddy_index]);
        z.z_free[n - 1].push_back(z.z_base[index]);
        z.z_free[n - 1].push_back(z.z_base[buddy_index]);
        z.z_base[index].p_order = n - 1;
        z.z_base[buddy_index].p_order = n - 1;
    }

    /* NOTREACHED */
    panic("NOTREACHED");
    return NULL;
}

void page_zone_add(addr_t base, size_t length)
{
    /*
     * We'll need the following:
     *
     * - struct PAGE_ZONE with information regarding this zone
     * - [num_pages] bits, to see whether a page is used
     * - [num_pages] x (struct PAGE) to contain information for a given memory page
     */
    unsigned int num_pages = length / PAGE_SIZE;
    unsigned int bitmap_size = (num_pages + 7) / 8;
    unsigned int num_admin_pages =
        (sizeof(PageZone) + bitmap_size + (num_pages * sizeof(Page)) + PAGE_SIZE - 1) / PAGE_SIZE;
    DPRINTF(
        "%s: base=%p length=%u -> num_pages=%u, num_admin_pages=%u\n", __func__, base, length,
        num_pages, num_admin_pages);

    char* mem = static_cast<char*>(
        kmem_map(base, num_admin_pages * PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE));

    /* Initialize the page zone; initially, we'll just mark everything as allocated */
    PageZone& z = *reinterpret_cast<PageZone*>(mem);
    new (&z.z_lock) Spinlock;
    z.z_bitmap = mem + sizeof(z);
    for (int n = 0; n < PAGE_NUM_ORDERS; n++)
        z.z_free[n].clear();
    memset(z.z_bitmap, 0xff, bitmap_size);
    z.z_base = reinterpret_cast<Page*>(mem + bitmap_size + sizeof(z));
    z.z_num_pages = num_pages - num_admin_pages;
    z.z_avail_pages = 0;
    z.z_phys_addr = base + num_admin_pages * PAGE_SIZE;

    /* Create the page structures; we mark everything as a order 0 page */
    Page* p = z.z_base;
    for (unsigned int n = 0; n < z.z_num_pages; n++, p++) {
        p->p_zone = &z;
        p->p_order = 0;
    }

    /*
     * Now, free all chunks of memory. This is slow, we could do better but for
     * now it'll help guarantee that the implementation is correct.
     */
    for (int n = 0; n < z.z_num_pages; n++)
        page_free_index(z, 0, n);

    /* Add the zone to the list XXX there should be some lock on zones */
    zones.push_back(z);
}

addr_t Page::GetPhysicalAddress() const
{
    AssertSane();

    const auto index = this - p_zone->z_base;
    return p_zone->z_phys_addr + index * PAGE_SIZE;
}

Page* page_alloc_order(int order)
{
    /* XXX this function has no lock on zones */

    KASSERT(order >= 0 && order < PAGE_NUM_ORDERS, "order %d out of range", order);
    KASSERT(!zones.empty(), "no zones");

    for (auto& z : zones) {
        Page* page = page_alloc_zone(z, order);
        if (page != NULL)
            return page;
    }

    panic("page_alloc(): failed for order %d", order);
}

void* page_alloc_order_mapped(int order, Page*& p, int vm_flags)
{
    p = page_alloc_order(order);
    if (p == nullptr)
        return nullptr;
    return kmem_map(p->GetPhysicalAddress(), PAGE_SIZE << order, vm_flags);
}

Page* page_alloc_length(size_t length) { return page_alloc_order(bytes2order(length)); }

void* page_alloc_length_mapped(size_t length, Page*& p, int vm_flags)
{
    return page_alloc_order_mapped(bytes2order(length), p, vm_flags);
}

void page_get_stats(unsigned int* total_pages, unsigned int* avail_pages)
{
    /* XXX we need some lock on zones */
    KASSERT(!zones.empty(), "no zones");

    *total_pages = 0;
    *avail_pages = 0;
    for (auto& z : zones) {
        SpinlockGuard g(z.z_lock);
        *total_pages += z.z_num_pages;
        *avail_pages += z.z_avail_pages;
    }
}

#ifdef OPTION_KDB
static void page_dump(PageZone& z)
{
    kprintf(
        "page_dump: zone=%p total=%u avail=%u (%u KB of %u KB in use)\n", &z, z.z_num_pages,
        z.z_avail_pages, (z.z_num_pages - z.z_avail_pages) * (PAGE_SIZE / 1024),
        z.z_num_pages * (PAGE_SIZE / 1024));
    for (unsigned int order = 0; order < PAGE_NUM_ORDERS; order++) {
        kprintf(" order %u: ", order);
        int n = 0;
        for (auto& f : z.z_free[order])
            n++;
        kprintf("%d\n", n);
    }
}

const kdb::RegisterCommand kdbPages("pages", "Display page zones", [](int, const kdb::Argument*) {
    for (auto& z : zones) {
        page_dump(z);
    }
});
#endif

/* vim:set ts=2 sw=2: */
