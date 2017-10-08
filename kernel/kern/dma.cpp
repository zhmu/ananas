/*
 * Ananas DMA infrastructure; this is very closely modelled after NetBSD's busdma(9).
 */
#include <ananas/error.h>
#include "kernel/bio.h"
#include "kernel/dma.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/page.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "kernel-md/dma.h"

TRACE_SETUP;

struct DMA_TAG {
	/* Parent tag, if any */
	struct DMA_TAG* t_parent;

	/* Number of references to this tag */
	refcount_t t_refcount;

	/* Device we belong to */
	Ananas::Device* t_dev;

	/* Alignment to use, or zero for anything */
	int t_alignment;

	/* Minimum/maximum address DMA-able */
	dma_addr_t t_min_addr, t_max_addr;

	/* Maximum number of segments supported per transaction */
	int t_max_segs;

	/* Maximum size per segment */
	dma_size_t t_max_seg_size;
};

/*
 * DMA buffer; 
 */
struct DMA_BUFFER {
	dma_tag_t db_tag;
	dma_size_t db_size;
	dma_size_t db_seg_size;
	unsigned int db_num_segs;
	struct DMA_BUFFER_SEGMENT db_seg[0];
};

errorcode_t
dma_tag_create(dma_tag_t parent, Ananas::Device& dev, dma_tag_t* tag, int alignment, dma_addr_t min_addr, dma_addr_t max_addr, unsigned int max_segs, dma_size_t max_seg_size)
{
	KASSERT(max_segs > 0, "invalid segment count %u", max_segs);
	KASSERT(max_seg_size > 0, "invalid maximum segment size %u", max_seg_size);

	/*
	 * Walk the path up and see if any parent has stricter limitations than the
	 * caller; we'll honor these as we walk by them.
	 */
	for (dma_tag_t t = parent; t != NULL; t = t->t_parent) {
		if (t->t_alignment > alignment)
			alignment = t->t_alignment; /* greater alignment wins */
		if (t->t_min_addr > min_addr)
			min_addr = t->t_min_addr; /* largest minimal address wins */
		if (t->t_max_addr < max_addr)
			max_addr = t->t_max_addr;  /* smallest maximum address wins */
		if (t->t_max_segs < max_segs)
			max_segs  = t->t_max_segs; /* smallest number of segments wins */
		if (t->t_max_seg_size < max_seg_size)
			max_seg_size = t->t_max_seg_size; /* smallest maximum segment size wins */
	}

	/* We've got all restrictions in place; create the tag using these */
	auto t = new DMA_TAG;
	memset(t, 0, sizeof(*t));
	t->t_parent = parent;
	t->t_refcount = 1;
	t->t_dev = &dev;
	t->t_alignment = alignment;
	t->t_min_addr = min_addr;
	t->t_max_addr = max_addr;
	t->t_max_segs = max_segs;
	t->t_max_seg_size = max_seg_size;
	*tag = t;
	return ananas_success();
}

errorcode_t
dma_tag_destroy(dma_tag_t tag)
{
	struct DMA_TAG* parent = tag->t_parent;

	/* Remove a reference; if we still have more, we do nothing */
	if (--tag->t_refcount > 0)
		return ananas_success();

	/* Remove our tag and try to kill the parent */
	kfree(tag);
	return dma_tag_destroy(parent);
}

errorcode_t
dma_buf_alloc(dma_tag_t tag, dma_size_t size, dma_buf_t* buf)
{
	/* First step is to see how many segments we need */
	unsigned int num_segs = 1;
	dma_size_t seg_size = size;
	if (tag->t_max_seg_size != DMA_SEGS_MAX_SIZE) {
		num_segs = (size + tag->t_max_seg_size - 1) / tag->t_max_seg_size;
		seg_size = size / num_segs; /* XXX is this correct? */
	}
	if (num_segs > tag->t_max_segs)
		return ANANAS_ERROR(BAD_LENGTH);
	if (seg_size > tag->t_max_seg_size)
		return ANANAS_ERROR(BAD_LENGTH);

	/* Allocate the buffer itself... */
	auto b = static_cast<struct DMA_BUFFER*>(kmalloc(sizeof(struct DMA_BUFFER) + num_segs * sizeof(struct DMA_BUFFER_SEGMENT)));
	memset(b, 0, sizeof(*b) + num_segs * sizeof(struct DMA_BUFFER_SEGMENT));
	b->db_tag = tag;
	b->db_size = size;
	b->db_seg_size = seg_size;
	b->db_num_segs = num_segs;
	tag->t_refcount++;

	/* ...and any memory backed by it */
	errorcode_t err = ananas_success();
	for (unsigned int n = 0; ananas_is_success(err) && n < num_segs; n++) {
		struct DMA_BUFFER_SEGMENT* s = &b->db_seg[n];
		s->s_virt = page_alloc_length_mapped(seg_size, &s->s_page, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE);
		if (s->s_virt == NULL) {
			err = ANANAS_ERROR(OUT_OF_MEMORY);
			break;
		}
		s->s_phys = page_get_paddr(s->s_page);
	}

	if (ananas_is_success(err))
		*buf = b;
	else
		dma_buf_free(b);
	return err;
}

void
dma_buf_free(dma_buf_t buf)
{
	dma_tag_t tag = buf->db_tag;

	/*
	 * We need to be vigilant when freeing stuff; it may be that the buffer was
	 * partially initialized (as dma_buf_alloc() can call us)
	 */
	for (unsigned int n = 0; n < buf->db_num_segs; n++) {
		struct DMA_BUFFER_SEGMENT* s = &buf->db_seg[n];
		if (s->s_virt != NULL)
			kmem_unmap(s->s_virt, buf->db_seg_size);
		if (s->s_page != NULL)
			page_free(s->s_page);
	}
	kfree(buf);

	dma_tag_destroy(tag);
}

unsigned int
dma_buf_get_segments(dma_buf_t buf)
{
	return buf->db_num_segs;
}

dma_buf_seg_t
dma_buf_get_segment(dma_buf_t buf, unsigned int n)
{
	KASSERT(n < buf->db_num_segs, "invalid segment %u", n);

#if 0
	kprintf("dma_buf_get_segment(): buf=%p n=%d -> size=%p seg=%p num=%d ",
	 buf, n, buf->db_size, buf->db_seg_size, buf->db_num_segs);
	kprintf("--> seg: virt %p phys %p\n", buf->db_seg[n].s_virt, buf->db_seg[n].s_phys);
#endif
	return &buf->db_seg[n];
}

void
dma_buf_sync(dma_buf_t buf, DMA_SYNC_TYPE type)
{
}

errorcode_t
dma_buf_load(dma_buf_t buf, void* data, dma_size_t size, dma_load_func_t load, void* load_arg, int flags)
{
	dma_tag_t tag = buf->db_tag;

	addr_t phys = kmem_get_phys(data);
	if (phys >= tag->t_min_addr && phys + size <= tag->t_max_addr &&
	    size < tag->t_max_seg_size &&
	    (phys & tag->t_alignment) == 0) {
		/* This is excellent; the device can accept this buffer as-is */
		struct DMA_BUFFER_SEGMENT bs;
		bs.s_page = NULL;
		bs.s_phys = phys;
		load(load_arg, &bs, 1);

		return ananas_success();
	}

	panic("dma_buf_load(): phys %p rejected, FIXME", phys);
}

errorcode_t
dma_buf_load_bio(dma_buf_t buf, struct BIO* bio, dma_load_func_t load, void* load_arg, int flags)
{
	return dma_buf_load(buf, BIO_DATA(bio), bio->length, load, load_arg, flags);
}

/* vim:set ts=2 sw=2: */
