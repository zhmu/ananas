/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/util/algorithm.h>
#include <machine/param.h>
#include "kernel/device.h"
#include "kernel/irq.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/result.h"
#include "kernel-md/interrupts.h"
#include <cstdint>

namespace irq
{
    namespace flag
    {
        constexpr inline auto Pending = (1 << 0);
        constexpr inline auto InProgress = (1 << 1);
    }

    namespace
    {
        util::array<IRQ, MAX_IRQS> irqList;
        IRQSourceList irqSources;
        Spinlock spl_irqSources;

        // Number of stray IRQ's that occur before reporting stops
        constexpr inline int maxStrayCount = 10;

        // Must be called with irq.i_lock held
        ISTHandler* FindFreeISTHandlerSlot(IRQ& irq)
        {
            auto it = util::find_if(
                irq.i_istHandler, [](const auto& ih) { return ih.h_type == HandlerType::Unhandled; });
            return it != irq.i_istHandler.end() ? &*it : nullptr;
        }

        // Must be called with irq.i_lock held
        void RunIRQHandlers(IRQ& irq, const int no, register_t& state)
        {
            while(true) {
                bool awake_thread = false, handled = false;
                if (irq.i_irqHandler) {
                    irq.i_lock.UnlockUnpremptible(state);
                    irq.i_irqHandler->OnIRQ();
                    state = irq.i_lock.LockUnpremptible();
                    handled = true;
                } else {
                    for (const auto& handler : irq.i_istHandler) {
                        switch(handler.h_type) {
                            case HandlerType::IST:
                                awake_thread = true; // need to invoke the IST
                                break;
                        }
                    }
                }

                if (awake_thread) {
                    // Mask the interrupt source; the ithread will unmask it once done.
                    // Note that edge-triggered interrupts cannot be masked and this
                    // function will do nothing in such a case
                    auto is = irq.i_source;
                    is->Mask(no - is->GetFirstInterruptNumber());

                    // Awake the interrupt thread
                    irq.i_semaphore.Signal();
                } else if (!handled) {
                    kprintf("Stray irq %u on cpu %d, ignored\n", no, PCPU_GET(cpuid));
                    if (++irq.i_straycount == maxStrayCount)
                        kprintf("Not reporting stray irq %u anymore\n", no);
                }

                // If no other IRQ's riggered in the meantime, we are done
                if ((irq.i_flags & flag::Pending) == 0)
                    break;
                irq.i_flags &= ~flag::Pending;
            }
        }
    } // unnamed namespace

    void RegisterSource(IRQSource& source)
    {
        SpinlockUnpremptibleGuard g(spl_irqSources);

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
        SpinlockUnpremptibleGuard g(spl_irqSources);

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
            for (const auto& handler : i.i_istHandler)
                KASSERT(
                    handler.h_callback == nullptr, "irq %u still registered to this source", num);
            num++;
        }

        irqSources.remove(source);
    }

    namespace {
        void ithread(void* context)
        {
            const auto no = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(context));
            KASSERT(no < irqList.size(), "ithread for impossible irq %u", no);

            auto& i = irqList[no];
            auto is = i.i_source;
            KASSERT(is != nullptr, "ithread for irq %u without source fired", no);

            while (1) {
                i.i_semaphore.Wait();

                /* XXX We do need a lock here */
                bool handled = false;
                for (auto& handler : i.i_istHandler) {
                    if (handler.h_type == HandlerType::IST && handler.h_callback->OnIRQ() == IRQResult::Processed)
                        handled = true;
                }
                if (!handled && i.i_straycount < maxStrayCount) {
                    kprintf("stray irq %d reported by ist\n", no);
                    ++i.i_straycount;
                }

                /* Unmask the IRQ, we can handle it again now that the IST is done */
                is->Unmask(no - is->GetFirstInterruptNumber());
            }
        }
    }

    Result RegisterIRQ(unsigned int no, IIRQHandler& irqHandler)
    {
        if (no >= irqList.size())
            return Result::Failure(ERANGE);

        auto& i = irqList[no];
        SpinlockUnpremptibleGuard g(i.i_lock);
        auto is = i.i_source;
        KASSERT(is != nullptr, "irq %d does not have a source; cannot fire", no);
        KASSERT(i.i_irqHandler == nullptr, "irq %d already has an IRQ registered", no);
        i.i_irqHandler = &irqHandler;

        // Unmask the interrupt; the caller shouldn't worry about this
        is->Unmask(no - is->GetFirstInterruptNumber());
        return Result::Success();
    }

    Result RegisterIST(unsigned int no, Device* dev, IIRQCallback& callback)
    {
        if (no >= irqList.size())
            return Result::Failure(ERANGE);

        // Note that we can't use SpinlockUnpremptibleGuard here as we may need to
        // release and re-acquire the lock
        auto& i = irqList[no];
        auto state = i.i_lock.LockUnpremptible();
        auto is = i.i_source;
        KASSERT(is != nullptr, "irq %d does not have a source; cannot fire", no);
        KASSERT(i.i_irqHandler == nullptr, "irq %d already has an IRQ registered", no);

        // Locate a free slot for the handler
        auto handler = FindFreeISTHandlerSlot(i);
        if (handler == nullptr) {
            i.i_lock.UnlockUnpremptible(state);
            return Result::Failure(EEXIST);
        }

        handler->h_device = dev;
        handler->h_callback = &callback;
        handler->h_type = HandlerType::NotYet;

        // If we need to create the IST, do so here
        if (!i.i_thread) {
            // Release lock as we can't kthread_init() with it
            i.i_lock.UnlockUnpremptible(state);

            char thread_name[16];
            snprintf(thread_name, sizeof(thread_name) - 1, "irq-%d", no);
            thread_name[sizeof(thread_name) - 1] = '\0';
            if (auto result =
                    kthread_alloc(thread_name, &ithread, reinterpret_cast<void*>(no), i.i_thread);
                result.IsFailure())
                panic("cannot create irq thread");

            // XXX we should set a decent priority for the ithread
            i.i_thread->Resume();

            state = i.i_lock.LockUnpremptible();
        }

        // Allow the handler to be executed now that it is properly set up
        handler->h_type = HandlerType::IST;

        // Unmask the interrupt; the caller shouldn't worry about this
        is->Unmask(no - is->GetFirstInterruptNumber());
        i.i_lock.UnlockUnpremptible(state);
        return Result::Success();
    }

    void InvokeHandler(unsigned int no)
    {
        const auto cpuid = PCPU_GET(cpuid);
        KASSERT(no < irqList.size(), "trying to handle out-of-range irq %u on cpu %d", cpuid, no, cpuid);
        auto& i = irqList[no];

        auto state = i.i_lock.LockUnpremptible();
        auto is = i.i_source;
        KASSERT(is != nullptr, "trying to handle irq %u without source", no);

        // Acknowledge the IRQ and set it to 'pending'; this means we've
        // seen an IRQ coming up and will deal with it
        is->Acknowledge(no - is->GetFirstInterruptNumber());
        i.i_flags |= flag::Pending;
        ++i.i_count;

        if ((i.i_flags & flag::InProgress) == 0) {
            // We are handling the IRQ for the first time; do it
            i.i_flags &= ~flag::Pending;
            i.i_flags |= flag::InProgress;

            RunIRQHandlers(i, no, state);
            i.i_flags &= ~flag::InProgress;
        }

        i.i_lock.UnlockUnpremptible(state);

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
        auto& curThread = thread::GetCurrent();
        if (irq_nestcount == 0 && curThread.IsRescheduling())
            scheduler::Schedule();
    }

} // namespace irq

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
        for (auto& handler : i.i_istHandler) {
            if (handler.h_type == irq::HandlerType::Unhandled)
                continue;
            if (!banner) {
                kprintf(
                    " IRQ %d count %u stray %d\n", no, i.i_count,
                    i.i_straycount);
                banner = 1;
            }
            kprintf(
                "  device '%s' handler %p type %d\n",
                (handler.h_device != nullptr) ? handler.h_device->d_Name : "<none>",
                handler.h_callback, static_cast<int>(handler.h_type));
        }
        if (!banner && (i.i_count > 0 || i.i_straycount > 0))
            kprintf(
                " IRQ %d count %u stray %d\n", no, i.i_count, i.i_straycount);
        no++;
    }
});
