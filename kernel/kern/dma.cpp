/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * Ananas DMA infrastructure; this is very closely modelled after NetBSD's busdma(9).
 */
#include <ananas/errno.h>
#include "kernel/bio.h"
#include "kernel/dma.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/page.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "kernel-md/dma.h"

namespace dma
{
    namespace detail
    {
        struct Impl {
            static Tag* CreateTag(
                Tag* parent, int alignment, dma_addr_t min_addr, dma_addr_t max_addr,
                unsigned int max_segs, dma_size_t max_seg_size)
            {
                KASSERT(max_segs > 0, "invalid segment count %u", max_segs);
                KASSERT(max_seg_size > 0, "invalid maximum segment size %u", max_seg_size);

                /*
                 * Walk the path up and see if any parent has stricter limitations than the
                 * caller; we'll honor these as we walk by them.
                 */
                for (auto t = parent; t != nullptr; t = t->t_parent) {
                    if (t->t_alignment > alignment)
                        alignment = t->t_alignment; // greater alignment wins
                    if (t->t_min_addr > min_addr)
                        min_addr = t->t_min_addr; // largest minimal address wins
                    if (t->t_max_addr < max_addr)
                        max_addr = t->t_max_addr; // smallest maximum address wins
                    if (t->t_max_segs < max_segs)
                        max_segs = t->t_max_segs; // smallest number of segments wins
                    if (t->t_max_seg_size < max_seg_size)
                        max_seg_size = t->t_max_seg_size; // smallest maximum segment size wins
                }

                // We've got all restrictions in place; create the tag using these
                return new Tag(parent, alignment, min_addr, max_addr, max_segs, max_seg_size);
            }

            static void Release(Tag& tag) { tag.Release(); }
        };

    } // namespace detail

    Tag::Tag(
        Tag* parent, int alignment, dma_addr_t min_addr, dma_addr_t max_addr, int max_segs,
        dma_size_t max_seg_size)
        : t_parent(parent), t_refcount(1), t_alignment(alignment), t_min_addr(min_addr),
          t_max_addr(max_addr), t_max_segs(max_addr), t_max_seg_size(max_seg_size)
    {
    }

    Tag* CreateTag(
        Tag* parent, int alignment, dma_addr_t min_addr, dma_addr_t max_addr, unsigned int max_segs,
        dma_size_t max_seg_size)
    {
        return detail::Impl::CreateTag(
            parent, alignment, min_addr, max_addr, max_segs, max_seg_size);
    }

    void Tag::Release()
    {
        auto parent = t_parent;

        // Drop a reference; if we still have more, we do nothing
        if (--t_refcount > 0)
            return;

        // Remove our tag and try to kill the parent
        delete this;
        parent->Release();
    }

    Buffer::Buffer(Tag& tag, dma_size_t size, dma_size_t seg_size, size_t num_segs)
        : db_tag(tag), db_size(size), db_seg_size(seg_size)
    {
        db_seg.resize(num_segs);
    }

    Result Tag::AllocateBuffer(dma_size_t size, Buffer*& buf)
    {
        /* First step is to see how many segments we need */
        unsigned int num_segs = 1;
        dma_size_t seg_size = size;
        if (t_max_seg_size != limits::maxSegmentSize) {
            num_segs = (size + t_max_seg_size - 1) / t_max_seg_size;
            seg_size = size / num_segs; /* XXX is this correct? */
        }
        if (num_segs > t_max_segs)
            return Result::Failure(EINVAL);
        if (seg_size > t_max_seg_size)
            return Result::Failure(EINVAL);

        /* Allocate the buffer itself... */
        auto b = new Buffer(*this, size, seg_size, num_segs);
        t_refcount++;

        /* ...and any memory backed by it */
        Result result = Result::Success();
        for (unsigned int n = 0; result.IsSuccess() && n < num_segs; n++) {
            auto& s = b->db_seg[n];
            s.s_virt = page_alloc_length_mapped(
                seg_size, s.s_page, vm::flag::Read | vm::flag::Write | vm::flag::Device);
            if (s.s_virt == NULL) {
                result = Result::Failure(ENOMEM);
                break;
            }
            s.s_phys = s.s_page->GetPhysicalAddress();
        }

        if (result.IsSuccess())
            buf = b;
        else
            delete b;
        return result;
    }

    Buffer::~Buffer()
    {
        /*
         * We need to be vigilant when freeing stuff; it may be that the buffer was
         * partially initialized (as dma_buf_alloc() can call us)
         */
        for (auto& s : db_seg) {
            if (s.s_virt != nullptr)
                kmem_unmap(s.s_virt, db_seg_size);
            if (s.s_page != nullptr)
                page_free(*s.s_page);
        }

        detail::Impl::Release(db_tag);
    }

    void Buffer::Synchronise(Sync type) {}

    Result
    Buffer::Load(void* data, dma_size_t size, dma_load_func_t load, void* load_arg, int flags)
    {
        addr_t phys = kmem_get_phys(data);
        if (phys >= db_tag.t_min_addr && phys + size <= db_tag.t_max_addr &&
            size < db_tag.t_max_seg_size && (phys & db_tag.t_alignment) == 0) {
            /* This is excellent; the device can accept this buffer as-is */
            BufferSegment bs;
            bs.s_page = NULL;
            bs.s_phys = phys;
            load(load_arg, &bs, 1);

            return Result::Success();
        }

        panic("dma_buf_load(): phys %p rejected, FIXME", phys);
    }

    Result Buffer::LoadBIO(struct BIO* bio, dma_load_func_t load, void* load_arg, int flags)
    {
        return Load(bio->Data(), bio->b_length, load, load_arg, flags);
    }

} // namespace dma
