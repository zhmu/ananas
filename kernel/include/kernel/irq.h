/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"
#include <ananas/util/list.h>
#include <ananas/util/array.h>
#include "kernel/thread.h"

class Device;
class Result;
struct STACKFRAME;

namespace irq
{
    /* Return values for the IST handler */
    enum class IRQResult { Ignored, Processed };

    // IST's use a dedicated thread to schedule the handlers; ISR's run immediately
    enum class HandlerType { Unhandled, NotYet, IST };

    class IIRQHandler
    {
      public:
        virtual void OnIRQ(STACKFRAME&) = 0;
    };

    class IIRQCallback
    {
      public:
        virtual IRQResult OnIRQ() = 0;
    };

    /*
     * Describes an IRQ source; this is generally an interrupt controller - every
     * source is responsible for a number of interrupts; these will be numbered
     * [ first .. first + count ], where 'first = GetFirstInterruptNumber()' and
     * 'count = GetInterruptCount()'. Callbacks are always issued using the
     * relative interrupt number, i.e. using [ 0 .. count ].
     */
    struct IRQSource : util::List<IRQSource>::NodePtr {
        // Retrieve the first interrupt handled
        virtual int GetFirstInterruptNumber() const = 0;
        // Retrieve the number of interrupts handled
        virtual int GetInterruptCount() const = 0;
        // Mask a given interrupt
        virtual void Mask(int) = 0;
        // Unmask a given interrupt
        virtual void Unmask(int) = 0;
        // Acknowledge a given interrupt
        virtual void Acknowledge(int) = 0;
    };
    using IRQSourceList = util::List<IRQSource>;

    /* Single IST handler */
    struct ISTHandler {
        Device* h_device = nullptr;
        IIRQCallback* h_callback = nullptr;
        HandlerType h_type{HandlerType::Unhandled};
    };

    /*
     * Describes an IRQ in machine-independant context.
     */
    static inline constexpr int MaxHandlersPerIRQ = 4;
    struct IRQ {
        Spinlock i_lock;
        IRQSource* i_source{};
        IIRQHandler* i_irqHandler{};
        util::array<ISTHandler, MaxHandlersPerIRQ> i_istHandler{};
        unsigned int i_flags{};
        unsigned int i_count{};
        unsigned int i_straycount{};
        Thread* i_thread{};
        Semaphore i_semaphore{"isem", 0};
    };

    /*
     * Note: on registering or removing an IRQ source, all interrupts are expected
     * to be masked.
     */
    void RegisterSource(IRQSource& source);
    void UnregisterSource(IRQSource& source);

    Result RegisterIRQ(unsigned int no, IIRQHandler& irqHandler);
    Result RegisterIST(unsigned int no, Device* dev, IIRQCallback& istHandler);
    void Unregister(unsigned int no, Device* dev, ISTHandler& istHandler);
    void InvokeHandler(STACKFRAME&, unsigned int no);
    void Dump();

} // namespace irq
