/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * malloc() inspired by "The C Programming Language", 2nd Edition.
 */

#include <ananas/types.h>
#include <machine/param.h> // for PAGE_SIZE
#include <sys/mman.h>
#include "lib.h"

namespace
{
    union header {
        struct {
            union header* h_next;
            size_t h_size;
        };
        long align;
    };

    typedef union header Header;

    constexpr size_t s_MinUnits = PAGE_SIZE / sizeof(header);
    Header s_Base = {}; /* empty list to get started */
    Header* s_Free = nullptr;                         /* start of free list */

    char* get_memory(size_t bytes)
    {
        return static_cast<char*>(mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0));
    }

    Header* morecore(size_t units)
    {
        if (units < s_MinUnits)
            units = s_MinUnits;
        char* cp = get_memory(units * sizeof(Header));
        if (cp == (char*)-1)
            /* no space at all */
            return NULL;

        Header* h = (Header*)cp;
        h->h_size = units;
        free((void*)(h + 1));
        return s_Free;
    }

} // unnamed namespace

/* malloc: general-purpose storage allocator */
void* malloc(size_t nbytes)
{
    size_t nunits = (nbytes + sizeof(Header) - 1) / sizeof(header) + 1;
    Header* prevp = s_Free;
    if (prevp == nullptr) {
        // No free list yet
        s_Base.h_next = &s_Base;
        s_Base.h_size = 0;
        s_Free = &s_Base;
        prevp = &s_Base;
    }

    for (Header* p = prevp->h_next; /* nothing */; prevp = p, p = p->h_next) {
        if (p->h_size >= nunits) {
            // Big enough
            if (p->h_size == nunits) {
                // Fits exactly
                prevp->h_next = p->h_next;
            } else {
                // Allocate tail end
                p->h_size -= nunits;
                p += p->h_size;
                p->h_size = nunits;
            }
            s_Free = prevp;
            return (void*)(p + 1);
        }
        if (p == s_Free) {
            /* Wrapped around free list */
            p = morecore(nunits);
            if (p == nullptr)
                return nullptr; /* none left */
        }
    }
}

/* free: put block ap in free list */
void free(void* ap)
{
    if (ap == NULL)
        return;
    Header* bp = static_cast<Header*>(ap) - 1;

    /* point to block header */
    Header* p = s_Free;
    for (/* nothing */; !(bp > p && bp < p->h_next); p = p->h_next)
        if (p >= p->h_next && (bp > p || bp < p->h_next)) {
            // Freed block at start or end of arena
            break;
        }

    if (bp + bp->h_size == p->h_next) {
        /* join to upper nbr */
        bp->h_size += p->h_next->h_size;
        bp->h_next = p->h_next->h_next;
    } else
        bp->h_next = p->h_next;

    if (p + p->h_size == bp) {
        /* join to lower nbr */
        p->h_size += bp->h_size;
        p->h_next = bp->h_next;
    } else
        p->h_next = bp;

    s_Free = p;
}
