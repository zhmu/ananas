#include <ananas/types.h>
#include "kernel/lib.h"
#include "kernel/kdb.h"
#include "kernel/page.h"
#include "kernel/lock.h"
#include "kernel/kmem.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "kernel-md/vm.h"
#include "kernel-md/param.h" // for PAGE_SIZE
#include "options.h"

namespace {

Mutex mtx_mm("mm");

struct MallocChunk
{
	MallocChunk() = default;
	MallocChunk(Page* p, void* virt, size_t len)
	 : mc_page(p), mc_virt(virt), mc_len(len)
	{
	}

	bool InUse() const
	{
		return mc_page != nullptr;
	}

	Page* mc_page{};
	void* mc_virt{};
	size_t mc_len{};
};

int mallocChunksTotal = 0;
Page* mallocChunksPage = nullptr;
MallocChunk* mallocChunks = nullptr;

}

extern "C" {
void* dlmalloc(size_t);
void dlfree(void*);
}

void*
kmalloc(size_t len)
{
	MutexGuard g(mtx_mm);
	return dlmalloc(len);
}

void
kfree(void* addr)
{
	MutexGuard g(mtx_mm);
	dlfree(addr);
}

void*
dlmalloc_get_memory(size_t len)
{
	mtx_mm.AssertLocked();

	Page* p;
	void* v = page_alloc_length_mapped(len, p, VM_FLAG_READ | VM_FLAG_WRITE);
	KASSERT(v != NULL, "out of memory allocating %u bytes", len);

	// Grab memory for the page administration if we don't yet have anything
	if (mallocChunksTotal == 0) {
		mallocChunksTotal = PAGE_SIZE / sizeof(MallocChunk); // XXX rough guess
		mallocChunks = static_cast<MallocChunk*>(page_alloc_length_mapped(PAGE_SIZE, mallocChunksPage, VM_FLAG_READ | VM_FLAG_WRITE));
		KASSERT(mallocChunks != nullptr, "unable to allocate");

		for (int n = 0; n < mallocChunksTotal; n++)
			new (&mallocChunks[n]) MallocChunk;
	}

	// Find an empty spot and insert our administration
	for (int n = 0; n < mallocChunksTotal; n++) {
		auto& mc = mallocChunks[n];
		if (mc.InUse())
			continue;

		// Got a spot; use it
		mc = MallocChunk(p, v, len);
		return v;
	}

	// FIXME - we could resize
	panic("out of spots in mallocChunks (count %d)", mallocChunksTotal);
}

int
dlmalloc_free_memory(void* ptr, size_t len)
{
	mtx_mm.AssertLocked();

	for (int n = 0; n < mallocChunksTotal; n++) {
		auto& mc = mallocChunks[n];
		if (mc.mc_virt != ptr || mc.mc_len != len)
			continue;

		kmem_unmap(mc.mc_virt, mc.mc_len);
		page_free(*mc.mc_page);

		mc = MallocChunk();
		return 0;
	}

	panic("cannot find virt %p length %d", ptr, len);
	return -1; /* failure */
}

void*
operator new(size_t len)
{
	return kmalloc(len);
}

void*
operator new[](size_t len)
{
	return kmalloc(len);
}

void
operator delete(void* p) noexcept
{
	kfree(p);
}

void
operator delete[](void* p) noexcept
{
	kfree(p);
}

void
kmem_chunk_reserve(addr_t chunk_start, addr_t chunk_end, addr_t reserved_start, addr_t reserved_end, addr_t* out_start, addr_t* out_end)
{
	/*
	 * This function takes a chunk [ chunk_start .. chunk_end ] and returns up to
	 * two chunks [ out_start[n] .. out_end[n] ] such that [ reserved_start ..
	 * reserved_end ] will not have overlap in the returned chunks. This is
	 * useful when adding memory zones as the kernel parts must be removed.
	 * 
	 * There are no less than 6 cases, where:
	 *   cs = chunk_start, ce = chunk_end
	 *   rs = reserved_start, re = reserved_end
	 *
	 *                 cs            ce
   *                 +==============+
   *  (1) rs +------------------------------+ re
   *  (2)            |   rs +---------------+ re
   *  (3) rs +------------+ re      |
   *  (4)            |  rs +---+ re |
   *  (5) rs +--+ re |              |
   *  (6)            |              | rs +--+ re
	 *
	 */
	out_start[0] = chunk_start;
	out_end[0] = chunk_end;
	out_start[1] = 0;
	out_end[1] = 0;

  if (/* 5 */ chunk_start >= reserved_end ||
	    /* 6 */ chunk_end <= reserved_start) {
		/* nothing to split */
		return; 
	}

	if (/* 1 */ chunk_start >= reserved_start && chunk_end <= reserved_end) {
		out_start[0] = 0;
		out_end[0] = 0;
		return;
	}
	if (/* 2 */ chunk_start < reserved_start && chunk_end <= reserved_end) {
		out_end[0] = reserved_start;
		return;
	}
	if (/* 3 */ chunk_start >= reserved_start && chunk_end > reserved_end) {
		out_start[0] = reserved_end;
		return;
	}

	/* 4 is the tricky case - two outputs! */
	KASSERT(chunk_start <= reserved_start && chunk_end >= reserved_end, "missing case c=%x/%x r=%x/%x", chunk_start, chunk_end, reserved_start, reserved_end);
	out_end[0] = reserved_start;
	out_start[1] = reserved_end;
	out_end[1] = chunk_end;
}

#ifdef OPTION_KDB
KDB_COMMAND(malloc, NULL, "Display malloc chunks")
{
	MutexGuard g(mtx_mm);

	size_t total_len = 0;
	for (int n = 0; n < mallocChunksTotal; n++) {
		auto& mc = mallocChunks[n];
		if (!mc.InUse())
			continue;

		kprintf("virt %16p phys %16p page %16p len %d\n",
		 mc.mc_virt, mc.mc_page->GetPhysicalAddress(), mc.mc_page, mc.mc_len);
		total_len += mc.mc_len;
	}
	kprintf("Total amount of memory used by malloc(): %d KB\n", total_len / 1024);
}
#endif

/* vim:set ts=2 sw=2: */
