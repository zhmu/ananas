/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/shutdown.h"
#include "kernel/cdefs.h"
#include "kernel/bio.h"
#include "kernel/lib.h"
#include "kernel/init.h"
#include "kernel/thread.h"
#include "kernel-md/md.h"
#include "kernel-md/interrupts.h"

namespace shutdown
{
    namespace
    {
        Thread* shutdown_thread{};
        ShutdownType shutdownType = ShutdownType::Unknown;

        constexpr int shutdownDelayInSeconds = 1;

        [[noreturn]] void PerformShutdown(void*);

        void PerformShutdown(void*)
        {
            const char* shutdownTypeText = [](ShutdownType st) {
                switch (st) {
                    case ShutdownType::Halt:
                        return "halt";
                    case ShutdownType::Reboot:
                        return "reboot";
                    case ShutdownType::PowerDown:
                        return "powerdown";
                    default:
                        panic("unknown shutdown type");
                }
            }(shutdownType);

            // XXX Kill processes, etc

            kprintf("Performing %s in %d second(s)...\n", shutdownTypeText, shutdownDelayInSeconds);
            thread_sleep_ms(shutdownDelayInSeconds * 1000);

            // XXX Unmount filesystems, etc
            kprintf("Synchronising BIO's...");
            bsync();
            kprintf("done\n");

            kprintf("Performing %s NOW!\n", shutdownTypeText);
            switch (shutdownType) {
                case ShutdownType::Reboot:
                    md::Reboot();
                    break;
                case ShutdownType::PowerDown:
                    md::PowerDown();
                    break;
                case ShutdownType::Halt:
                default:
                    break;
            }

            // If we survived any of that, hang
            md::interrupts::Disable();
            for (;;)
                md::interrupts::Relax();
            // NOTREACHED
        }

    } // unnamed namespace

    bool IsShuttingDown()
    {
        return shutdownType != ShutdownType::Unknown && shutdown_thread->t_state != thread::State::Suspended;
    }

    void RequestShutdown(ShutdownType type)
    {
        KASSERT(type != ShutdownType::Unknown, "requested unknown shutdown");

        // XXX Maybe we need some locking here
        if (IsShuttingDown())
            return;
        shutdownType = type;
        shutdown_thread->Resume();
    }

} // namespace shutdown

namespace
{
    const init::OnInit initShutdown(init::SubSystem::Thread, init::Order::Middle, []() {
        using namespace shutdown;
        if (auto result = kthread_alloc("shutdown", &PerformShutdown, NULL, shutdown_thread);
            result.IsFailure())
            panic("cannot create shutdown thread");
    });

} // unnamed namespace
