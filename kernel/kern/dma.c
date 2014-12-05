/*
 * Ananas DMA infrastructure; this is very closely modelled after NetBSD's busdma(9).
 */
#include <ananas/dma.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/kmem.h>
#include <ananas/mm.h>
#include <ananas/page.h>
#include <ananas/vm.h>
#include <machine/dma.h>
#include <machine/param.h>

TRACE_SETUP;

struct DMA_TAG {
	/* Parent tag, if any */
	struct DMA_TAG* t_parent;

	/* Number of references to this tag */
	refcount_t t_refcount;

	/* Device we belong to */
	device_t t_dev;

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
dma_tag_create(dma_tag_t parent, device_t dev, dma_tag_t* tag, int alignment, dma_addr_t min_addr, dma_addr_t max_addr, unsigned int max_segs, dma_size_t max_seg_size)
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
	struct DMA_TAG* t = kmalloc(sizeof(*t));
	memset(t, 0, sizeof(*t));
	t->t_parent = parent;
	t->t_refcount = 1;
	t->t_dev = dev;
	t->t_alignment = alignment;
	t->t_min_addr = min_addr;
	t->t_max_addr = max_addr;
	t->t_max_segs = max_segs;
	t->t_max_seg_size = max_seg_size;
	*tag = t;
	return ANANAS_ERROR_NONE;
}

errorcode_t
dma_tag_destroy(dma_tag_t tag)
{
	struct DMA_TAG* parent = tag->t_parent;

	/* Remove a reference; if we still have more, we do nothing */
	if (--tag->t_refcount > 0)
		return ANANAS_ERROR_NONE;

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
	struct DMA_BUFFER* b = kmalloc(sizeof(*b) + num_segs * sizeof(struct DMA_BUFFER_SEGMENT));
	memset(b, 0, sizeof(*b) + num_segs * sizeof(struct DMA_BUFFER_SEGMENT));
	b->db_tag = tag;
	b->db_size = size;
	b->db_seg_size = seg_size;
	b->db_num_segs = num_segs;
	tag->t_refcount++;

	/* ...and any memory backed by it */
	errorcode_t err = ANANAS_ERROR_NONE;
	for (unsigned int n = 0; err == ANANAS_ERROR_NONE && n < num_segs; n++) {
		struct DMA_BUFFER_SEGMENT* s = &b->db_seg[n];
		s->s_virt = page_alloc_length_mapped(seg_size, &s->s_page, VM_FLAG_READ | VM_FLAG_WRITE);
		if (s->s_virt == NULL) {
			err = ANANAS_ERROR(OUT_OF_MEMORY);
			break;
		}
		s->s_phys = page_get_paddr(s->s_page);
	}

	if (err == ANANAS_ERROR_NONE)
		*buf = b;
	else
		dma_buf_free(b);
	return err;
}

void
dma_buf_free(dma_buf_t buf)
{
	dma_tag_t tag = buf->db_tag;

	for (unsigned int n = 0; n < buf->db_num_segs; n++) {
		struct DMA_BUFFER_SEGMENT* s = &buf->db_seg[n];
		if (s->s_page == NULL)
			continue; /* may also be called by dma_alloc_buffer() on failure */

		kmem_unmap(s->s_virt, buf->db_seg_size);
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
	return &buf->db_seg[n];
}

void
dma_buf_sync(dma_buf_t buf, DMA_SYNC_TYPE type)
{
}


/* vim:set ts=2 sw=2: */
