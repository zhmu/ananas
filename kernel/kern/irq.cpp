/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/util/algorithm.h>
#include <machine/param.h>
#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/irq.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel-md/interrupts.h"
#include "options.h"

TRACE_SETUP;

namespace irq
{
    namespace
    {
        util::array<IRQ, MAX_IRQS> irqList;
        IRQSourceList irqSources;
        Spinlock spl_irq;

        // Number of stray IRQ's that occur before reporting stops
        constexpr inline int maxStrayCount = 10;

        // Must be called with spl_irq held
        IRQSource* FindSource(unsigned int num)
        {
            auto it = util::find_if(irqSources, [&](const IRQSource& is) {
                return num >= is.GetFirstInterruptNumber() &&
                       num <= is.GetFirstInterruptNumber() + is.GetInterruptCount();
            });
            return it != irqSources.end() ? &*it : nullptr;
        }

        // Must be called with spl_irq held
        IRQHandler* FindFreeHandlerSlot(IRQ& irq)
        {
            auto it = util::find_if(
                irq.i_handler, [](const IRQHandler& ih) { return ih.h_handler == nullptr; });
            return it != irq.i_handler.end() ? &*it : nullptr;
        }

    } // unnamed namespace

    void RegisterSource(IRQSource& source)
    {
        SpinlockUnpremptibleGuard g(spl_irq);

        /* Ensure no bogus ranges are being registered */
        const auto source_irq_first = source.GetFirstInterruptNumber();
        const auto source_irq_count = source.GetInterruptCount();
        KASSERT(source_irq_count >= 1, "must register at least one irq");
        KASSERT(
            source_irq_first + source_irq_count < irqList.size(),
            "can't register beyond irqList range");

        /* Ensure there will not be an overlap */
        for (auto& is : irqSources) {
            const auto is_irq_first = is.GetFirstInterruptNumber();
            const auto is_irq_count = is.GetInterruptCount();
            KASSERT(
                source_irq_first + source_irq_count < is_irq_first ||
                    is_irq_first + is_irq_count < source_irq_first,
                "overlap in interrupt ranges (have %u-%u, attempt to add %u-%u)", is_irq_first,
                is_irq_first + is_irq_count, source_irq_first, source_irq_first + source_irq_count);
        }
        irqSources.push_back(source);

        /* Hook all IRQ's to this source - this saves having to look things up later */
        for (unsigned int n = 0; n < source_irq_count; n++) {
            auto& i = irqList[source_irq_first + n];
            i.i_source = &source;
        }
    }

    void UnregisterSource(IRQSource& source)
    {
        SpinlockUnpremptibleGuard g(spl_irq);

        KASSERT(!irqSources.empty(), "no irq sources registered");
        /* Ensure our source is registered */
        int matches = 0;
        for (const auto& is : irqSources) {
            if (&is != &source)
                continue;
            matches++;
        }
        KASSERT(matches == 1, "irq source not registered");

        /* Walk through the IRQ's and ensure they do not use this source anymore */
        int num = 0;
        for (auto& i : irqList) {
            if (i.i_source != &source) {
                num++;
                continue;
            }
            i.i_source = nullptr;
            for (const auto& handler : i.i_handler)
                KASSERT(
                    handler.h_handler == nullptr, "irq %u still registered to this source", num);
            num++;
        }

        irqSources.remove(source);
    }

    static void ithread(void* context)
    {
        unsigned int no = (unsigned int)(uintptr_t)context;
        KASSERT(no < irqList.size(), "ithread for impossible irq %u", no);

        auto& i = irqList[no];
        auto is = i.i_source;
        KASSERT(is != nullptr, "ithread for irq %u without source fired", no);

        while (1) {
            i.i_semaphore.Wait();

            /* XXX We do need a lock here */
            for (auto& handler : i.i_handler) {
                if (handler.h_handler == nullptr || (handler.h_flags & IRQ_HANDLER_FLAG_SKIP))
                    continue;
                if (handler.h_flags & IRQ_HANDLER_FLAG_THREAD)
                    handler.h_handler
                        ->OnIRQ(); // XXX Maybe we should care about the return code here
            }

            /* Unmask the IRQ, we can handle it again now that the IST is done */
            is->Unmask(no - is->GetFirstInterruptNumber());
        }
    }

    Result Register(unsigned int no, Device* dev, int type, IHandler& irqHandler)
    {
        if (no >= irqList.size())
            return RESULT_MAKE_FAILURE(ERANGE);

        // Note that we can't use SpinlockUnpremptibleGuard here as we may need to
        // release and re-acquire the lock
        auto state = spl_irq.LockUnpremptible();

        /*
         * Look up the interrupt source; if we can't find it, it means this interrupt will
         * never fire so we should refuse to register it.
         */
        IRQSource* is = FindSource(no);
        if (is == nullptr) {
            spl_irq.UnlockUnpremptible(state);
            return RESULT_MAKE_FAILURE(ENODEV);
        }

        auto& i = irqList[no];
        i.i_source = is;

        // Locate a free slot for the handler
        IRQHandler* handler = FindFreeHandlerSlot(i);
        if (handler == nullptr) {
            // XXX does it matter that we set i_source before?
            spl_irq.UnlockUnpremptible(state);
            return RESULT_MAKE_FAILURE(EEXIST);
        }

        // Found one, hook it up
        handler->h_device = dev;
        handler->h_handler = &irqHandler;
        handler->h_flags = 0;
        if (type != IRQ_TYPE_ISR)
            handler->h_flags |= IRQ_FLAG_THREAD;

        // If we need to create the IST, do so here
        if ((i.i_flags & IRQ_HANDLER_FLAG_THREAD) == 0 && (handler->h_flags & IRQ_FLAG_THREAD)) {
            // Register the handler, but mark it as IRQ_HANDLER_FLAG_SKIP to avoid calling it
            handler->h_flags |= IRQ_HANDLER_FLAG_SKIP;

            // Release lock as we can't kthread_init() with it
            spl_irq.UnlockUnpremptible(state);

            // (3) Create the thread
            char thread_name[PAGE_SIZE];
            snprintf(thread_name, sizeof(thread_name) - 1, "irq-%d", no);
            thread_name[sizeof(thread_name) - 1] = '\0';
            kthread_init(i.i_thread, thread_name, &ithread, (void*)(uintptr_t)no);
            i.i_thread.Resume();

            // XXX we should set a decent priority here

            // (4) Re-acquire the lock
            state = spl_irq.LockUnpremptible();

            // (5) Remove the IRQ_SKIP flag, add IRQ_FLAG_THREAD so it will be serviced correctly
            handler->h_flags &= ~IRQ_HANDLER_FLAG_SKIP;
            i.i_flags |= IRQ_FLAG_THREAD;
        }

        spl_irq.UnlockUnpremptible(state);

        // Unmask the interrupt; the caller shouldn't worry about this
        is->Unmask(no - is->GetFirstInterruptNumber());
        return Result::Success();
    }

    void Unregister(unsigned int no, Device* dev, IHandler& irqHandler)
    {
        KASSERT(no < irqList.size(), "interrupt %u out of range", no);

        SpinlockUnpremptibleGuard g(spl_irq);
        auto& i = irqList[no];
        KASSERT(i.i_source != nullptr, "interrupt %u has no source", no);

        bool isUnregistered = false;
        for (auto& handler : i.i_handler) {
            if (handler.h_device != dev || handler.h_handler != &irqHandler)
                continue;

            /* Found a match; unregister it */
            handler.h_device = nullptr;
            handler.h_handler = nullptr;
            handler.h_flags = 0;
            isUnregistered = true;
        }

        KASSERT(isUnregistered, "interrupt %u not registered", no);

        // XXX We should mask the interrupt, if it has no consumers left
    }

    void InvokeHandler(unsigned int no)
    {
        int cpuid = PCPU_GET(cpuid);
        auto& i = irqList[no];
        i.i_count++;

        KASSERT(no < irqList.size(), "irq_handler: (CPU %u) impossible irq %u fired", cpuid, no);
        IRQSource* is = i.i_source;
        KASSERT(is != nullptr, "irq_handler(): irq %u without source fired", no);

        /*
         * Try all handlers one by one until we have one that works - if we find a handler that is
         * to be call from the IST, flag so we don't do that multiple times.
         */
        bool awake_thread = false, handled = false;
        for (auto& handler : i.i_handler) {
            if (handler.h_handler == nullptr || (handler.h_flags & IRQ_HANDLER_FLAG_SKIP))
                continue;
            if ((handler.h_flags & IRQ_HANDLER_FLAG_THREAD) == 0) {
                // Plain old ISR
                if (handler.h_handler->OnIRQ() == IRQResult::Processed)
                    handled = true;
            } else {
                awake_thread = true; // need to invoke the IST
            }
        }

        if (awake_thread) {
            // Mask the interrupt source; the ithread will unmask it once done
            is->Mask(no - is->GetFirstInterruptNumber());

            // Awake the interrupt thread
            i.i_semaphore.Signal();
        } else if (!handled && i.i_straycount < maxStrayCount) {
            // If they IRQ wasn't handled, it is stray
            kprintf("(CPU %u) Stray irq %u, ignored\n", cpuid, no);
            if (++i.i_straycount == maxStrayCount)
                kprintf("Not reporting stray irq %u anymore\n", no);
        }

        /*
         * Disable interrupts; as we were handling an interrupt, this means we've
         * been interrupted. We'll want to clean up, because if another interrupt
         * occurs, it'll just expand the current context - and if interrupts come
         * quickly enough, we'll run out of stack and crash 'n burn.
         */
        md::interrupts::Disable();

        // Acknowledge the interrupt once the handler is done
        is->Acknowledge(no - is->GetFirstInterruptNumber());

        /*
         * Decrement the IRQ nesting counter; interrupts are disabled, so it's safe
         * to do this - if the nesting counter reaches zero, we can do a reschedule
         * if necessary.
         */
        int irq_nestcount = PCPU_GET(nested_irq);
        KASSERT(irq_nestcount > 0, "IRQ nesting too low while in irq!");
        irq_nestcount--;
        PCPU_SET(nested_irq, irq_nestcount);

        // If the IRQ handler resulted in a reschedule of the current thread, handle it
        Thread* curthread = PCPU_GET(curthread);
        if (irq_nestcount == 0 && curthread->IsRescheduling())
            scheduler::Schedule();
    }

} // namespace irq

#ifdef OPTION_KDB
const kdb::RegisterCommand kdbIRQ("irq", "Display IRQ status", [](int, const kdb::Argument*) {
    /* Note: no need to grab locks as the debugger runs with interrupts disabled */
    kprintf("Registered IRQ sources:\n");
    for (const auto& is : irq::irqSources) {
        kprintf(
            " IRQ %d..%d\n", is.GetFirstInterruptNumber(),
            is.GetFirstInterruptNumber() + is.GetInterruptCount());
    }

    kprintf("IRQ handlers:\n");
    int no = 0;
    for (auto& i : irq::irqList) {
        int banner = 0;
        for (auto& handler : i.i_handler) {
            if (handler.h_handler == nullptr)
                continue;
            if (!banner) {
                kprintf(
                    " IRQ %d flags %x count %u stray %d\n", no, i.i_flags, i.i_count,
                    i.i_straycount);
                banner = 1;
            }
            kprintf(
                "  device '%s' handler %p flags %x\n",
                (handler.h_device != nullptr) ? handler.h_device->d_Name : "<none>",
                handler.h_handler, handler.h_flags);
        }
        if (!banner && (i.i_flags != 0 || i.i_count > 0 || i.i_straycount > 0))
            kprintf(
                " IRQ %d flags %x count %u stray %d\n", no, i.i_flags, i.i_count, i.i_straycount);
        no++;
    }
});
#endif
