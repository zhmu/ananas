#ifndef __IRQ_H__
#define __IRQ_H__

#include <ananas/types.h>
#include <ananas/util/list.h>
#include <ananas/util/array.h>
#include "kernel/thread.h"

class Device;
class Result;

namespace irq
{
    /* Return values for the IRQ handler */
    enum class IRQResult { Ignored, Processed };

    class IHandler
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
    typedef util::List<IRQSource> IRQSourceList;

#define IRQ_MAX_HANDLERS 4

    /* Single IRQ handler */
    struct IRQHandler {
        Device* h_device = nullptr;
        IHandler* h_handler = nullptr;
        unsigned int h_flags = 0;
#define IRQ_HANDLER_FLAG_THREAD (1 << 0) /* invoke the handler from the IST */
#define IRQ_HANDLER_FLAG_SKIP (1 << 1)   /* (internal use only) do not invoke the handler */
    };

    /*
     * Describes an IRQ in machine-independant context.
     */
    struct IRQ {
        IRQSource* i_source;
        util::array<IRQHandler, IRQ_MAX_HANDLERS> i_handler;
        unsigned int i_count;
        unsigned int i_straycount;
        unsigned int i_flags;
#define IRQ_FLAG_THREAD (1 << 0) /* execute this handler from a thread */
        Thread i_thread;
        Semaphore i_semaphore{0};
    };

    /*
     * Note: on registering or removing an IRQ source, all interrupts are expected
     * to be masked.
     */
    void RegisterSource(IRQSource& source);
    void UnregisterSource(IRQSource& source);

#define IRQ_TYPE_DEFAULT IRQ_TYPE_IST
#define IRQ_TYPE_IST 0 /* use an IST to launch the handler */
#define IRQ_TYPE_ISR 1 /* do not launch the handler from a thread */
#define IRQ_TYPE_IPI IRQ_TYPE_ISR
#define IRQ_TYPE_TIMER IRQ_TYPE_ISR
    Result Register(unsigned int no, Device* dev, int type, IHandler& irqHandler);
    void Unregister(unsigned int no, Device* dev, IHandler& irqHandler);
    void InvokeHandler(unsigned int no);
    void Dump();

} // namespace irq

#endif /* __IRQ_H__ */
