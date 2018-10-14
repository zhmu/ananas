#ifndef __ANANAS_DMA_H__
#define __ANANAS_DMA_H__

#include <ananas/types.h>
#include "kernel-md/dma.h"

class Device;
class Result;

#define DMA_ADDR_MAX_32BIT ((dma_addr_t)0xFFFFFFFF)
#define DMA_ADDR_MAX_ANY ((dma_addr_t)~0)
#define DMA_SEGS_MAX_ANY ((unsigned int)~0)
#define DMA_SEGS_MAX_SIZE ((dma_size_t)~0)

/* DMA tag; contains information how a given device needs DMA to work */
/* DMA buffer: contains a list of segments with data */
/* DMA buffer segment: contains a page to DMA from/to */

typedef enum {
	DMA_SYNC_IN,
	DMA_SYNC_OUT
} DMA_SYNC_TYPE;

struct Page;

struct DMA_BUFFER_SEGMENT {
	Page* s_page;
	void* s_virt;
	dma_addr_t s_phys;
};

/*
 * Creates a tag for a given device; this tag is used to perform DMA.
 *
 * parent: DMA tag of parent device
 * dev: Device to use the DMA tag for (and its children)
 * tag: On return, tag to use for these DMA properties
 * alignment: Alignment restrictions on the DMA buffer, in bytes
 * min_addr: Minimum address supported by the device
 * max_addr: Maximum address supported by the device
 * max_segs: Maximum number of segments supported by the device
 * max_seg_size: Maximum number of bytes per segment supported
 */
Result dma_tag_create(dma_tag_t parent, Device& dev, dma_tag_t* tag, int alignment, dma_addr_t min_addr, dma_addr_t max_addr, unsigned int max_segs, dma_size_t max_seg_size);

/* Destroys a given DMA tag */
Result dma_tag_destroy(dma_tag_t tag);

/*
 * Allocates a buffer intended for DMA to a given device.
 *
 * tag: DMA tag in use by the device
 */
Result dma_buf_alloc(dma_tag_t tag, dma_size_t size, dma_buf_t* buf);

/*
 * Frees a buffer
 */
void dma_buf_free(dma_buf_t buf);

/* Synchronize a DMA buffer prior to transmitting/post receiving data */
void dma_buf_sync(dma_buf_t buf, DMA_SYNC_TYPE type);

/* Retrieves the number of segments for a given buffer */
unsigned int dma_buf_get_segments(dma_buf_t buf);

/* Retrieve a given buffer's segment */
dma_buf_seg_t dma_buf_get_segment(dma_buf_t buf, unsigned int n);

typedef Result (*dma_load_func_t)(void* ctx, struct DMA_BUFFER_SEGMENT* s, int num_segs);

/* Loads a given buffer to DMA-able addresses for the device */
Result dma_buf_load(dma_buf_t buf, void* data, dma_size_t size, dma_load_func_t load, void* load_arg, int flags);

/* Loads a BIO buffer to DMA-able addresses for the device */
Result dma_buf_load_bio(dma_buf_t buf, struct BIO* bio, dma_load_func_t load, void* load_arg, int flags);

#endif /* __ANANAS_DMA_H__ */
