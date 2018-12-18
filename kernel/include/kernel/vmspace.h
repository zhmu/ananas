#ifndef ANANAS_VMSPACE_H
#define ANANAS_VMSPACE_H

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/page.h"
#include "kernel/vmpage.h"
#include "kernel-md/vmspace.h"

struct DEntry;
struct VMArea;
class Result;

/*
 * VM space describes a thread's complete overview of memory.
 */
struct VMSpace {
    void Lock() { vs_mutex.Lock(); }

    void Unlock() { vs_mutex.Unlock(); }

    void AssertLocked() { vs_mutex.AssertLocked(); }

    util::List<VMArea> vs_areas;

    /*
     * Contains pages allocated to the space that aren't part of a mapping; this
     * is mainly used to store MD-dependant things like pagetables.
     */
    PageList vs_pages;

    addr_t vs_next_mapping; /* address of next mapping */

    MD_VMSPACE_FIELDS

    Result MapTo(addr_t virt, size_t len /* bytes */, uint32_t flags, VMArea*& va_out);
    Result MapToDentry(
        addr_t virt, size_t vlength, DEntry& dentry, off_t doffset, size_t dlength, int flags,
        VMArea*& va_out);
    Result Map(size_t len /* bytes */, uint32_t flags, VMArea*& va_out);
    Result Clone(VMSpace& vs_dest);
    void Dump();

    addr_t ReserveAdressRange(size_t len);
    Result HandleFault(addr_t virt, int flags);

    void PrepareForExecute();

    // XXX Should not be public
    void FreeArea(VMArea& va);

  private:
    Mutex vs_mutex{"vmspace"}; /* protects all fields and sub-areas */
};

Result vmspace_create(VMSpace*& vs);
void vmspace_destroy(VMSpace& vs);

#endif /* ANANAS_VMSPACE_H */
