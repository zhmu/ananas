#ifndef __ANANAS_DMA_H__
#define __ANANAS_DMA_H__

#include <ananas/types.h>
#include <ananas/util/vector.h>
#include "kernel-md/dma.h"

class Device;
struct Page;
class Result;

namespace dma {
namespace detail {
struct Impl;
}

namespace limits {
constexpr auto minAddr = static_cast<dma_addr_t>(0);
constexpr auto maxAddr = static_cast<dma_addr_t>(~0);
constexpr auto max32BitAddr = static_cast<dma_addr_t>(0xffffffff);
constexpr auto maxSegments = static_cast<int>(~0);
constexpr auto maxSegmentSize = static_cast<dma_size_t>(~0);
constexpr auto anyAlignment = static_cast<int>(1);
}

enum class Sync {
	In,
	Out
};

struct BufferSegment final
{
	Page* s_page;
	void* s_virt;
	dma_addr_t s_phys;
};

class Buffer;
class Tag;
typedef Result (*dma_load_func_t)(void* ctx, BufferSegment* s, int num_segs);

class Buffer final
{
	friend class Tag;
public:
	~Buffer();

	// Synchronise a DMA buffer prior to transmitting/post receiving data
	void Synchronise(Sync type);

	// Retrieve a given buffer's segments
	util::vector<BufferSegment>& GetSegments() { return db_seg; }

	// Loads a given buffer to DMA-able addresses for the device */
	Result Load(void* data, dma_size_t size, dma_load_func_t load, void* load_arg, int flags);

	/* Loads a BIO buffer to DMA-able addresses for the device */
	Result LoadBIO(struct BIO* bio, dma_load_func_t load, void* load_arg, int flags);

private:
	Buffer(Tag& tag, dma_size_t size, dma_size_t seg_size, size_t num_segs);

	Tag& db_tag;
	dma_size_t db_size;
	dma_size_t db_seg_size;
	util::vector<BufferSegment> db_seg;
};

class Tag final
{
	friend class detail::Impl;
	friend class Buffer;
public:
	// Allocates a buffer intended for DMA to a given device.
	Result AllocateBuffer(dma_size_t size, Buffer*& buf);

private:
	// Parent tag, if any
	Tag* t_parent = nullptr;

	// Number of references to this tag
	refcount_t t_refcount;

	// Alignment to use, or zero for anything
	int t_alignment = 0;

	// Minimum/maximum address DMA-able
	dma_addr_t t_min_addr, t_max_addr;

	// Maximum number of segments supported per transaction
	int t_max_segs;

	// Maximum size per segment
	dma_size_t t_max_seg_size;

	Tag(Tag* parent, int alignment, dma_addr_t min_addr, dma_addr_t max_addr, int max_segs, dma_size_t max_seg_size);
	void Release();
};

Tag* CreateTag(Tag* parent, int alignment, dma_addr_t min_addr, dma_addr_t max_addr, unsigned int max_segs, dma_size_t max_seg_size);

} // namespace dma

#endif /* __ANANAS_DMA_H__ */
