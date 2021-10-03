/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/console.h"
#include "kernel/console-driver.h"
#include "kernel/cmdline.h"
#include "kernel/debug-console.h"
#include "kernel/device.h"
#include "kernel/mm.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/result.h"
#include "kernel/vfs/types.h"

namespace driver_manager::internal
{
    DriverList& GetDriverList();
} // namespace driver_manager::internal
namespace device_manager::internal
{
    Device* ProbeConsole(ConsoleDriver& driver);
} // namespace device_manager::internal

Device* console_tty = nullptr;

namespace {
    constexpr inline auto backlogSize = 1024;

    int console_mutex_inuse = 0;
    Spinlock console_backlog_lock;
    char* console_backlog;
    int console_backlog_pos = 0;
    int console_backlog_overrun = 0;
    Mutex mtx_console("console");

    VFS_FILE console_file{};

    bool console_attach(ConsoleDriver& consoleDriver)
    {
        /*
         * See if this devices probes. Note that we don't provide any resources
         * here - devices that require them can't be used as console devices.
         */
        Device* dev = device_manager::internal::ProbeConsole(consoleDriver);
        if (dev == nullptr)
            return false; // likely not present
        if (auto result = device_manager::AttachSingle(*dev); result.IsFailure())
            return false; // too bad; this driver won't work

        console_tty = dev;
        return true;
    }

    bool console_probe(const char* driverFilter = nullptr)
    {
        auto& drivers = driver_manager::internal::GetDriverList();
        for (auto& driver : drivers) {
            ConsoleDriver* consoleDriver = driver.GetConsoleDriver();
            if (consoleDriver == nullptr)
                continue; // not a console driver, skip

            if (driverFilter != nullptr && strcmp(consoleDriver->d_Name, driverFilter) != 0)
                continue; // doesn't match filter

            if (console_attach(*consoleDriver))
                return true;
        }

        return false;
    }

    const init::OnInit initConsole(init::SubSystem::Console, init::Order::Middle, []() {
        {
            const char* console_override = cmdline_get_string("console");
            if (console_override == nullptr || !console_probe(console_override)) {
                // Either no filter or filtered driver doesn't work - try them all
                console_probe();
            }
        }

        // Initialize the backlog; we use it to queue messages once the mutex is hold
        console_backlog = new char[backlogSize];
        console_backlog_pos = 0;

        // Initialize the console print mutex and start using it
        console_mutex_inuse++;
    });

} // unnamed namespace

void console_putchar(int c)
{
    debugcon_putch(c);
    if (console_tty == NULL)
        return;
    uint8_t ch = c; // cannot cast due to endianness!
    size_t len = sizeof(ch);

    console_tty->GetCharDeviceOperations()->Write(console_file, (const void*)&ch, len);
}

static void console_puts(const char* s)
{
    for (int c = *s++; c != 0; c = *s++)
        console_putchar(c);
}

void console_putstring(const char* s)
{
    if (console_mutex_inuse && !mtx_console.TryLock()) {
        SpinlockUnpremptibleGuard g(console_backlog_lock);

        /*
         * Couldn't obtain the console mutex; we should put our message in the
         * back log and wait for it to be picked up later.
         */
        int len = strlen(s);
        if (console_backlog_pos + len < backlogSize) {
            /* Fits! */
            strcpy(&console_backlog[console_backlog_pos], s);
            console_backlog_pos += len;
        } else {
            /* Doesn't fit */
            console_backlog_overrun++;
        }
        return;
    }
    console_puts(s);

    /* See if there's anything in the backlog that we can print */
    if (console_backlog_pos > 0) {
        SpinlockUnpremptibleGuard g(console_backlog_lock);
        for (int n = 0; n < console_backlog_pos; n++)
            console_putchar(console_backlog[n]);
        console_backlog_pos = 0;

        if (console_backlog_overrun)
            console_puts("[***OVERRUN***]");
    }

    if (console_mutex_inuse)
        mtx_console.Unlock();
}

uint8_t console_getchar()
{
    int ch = debugcon_getch();
    if (ch != 0)
        return ch;
    if (console_tty == nullptr)
        return 0;
    uint8_t c = 0;
    size_t len = sizeof(c);
    if (auto result = console_tty->GetCharDeviceOperations()->Read(console_file, (void*)&c, len);
        result.IsFailure())
        return 0;
    return c;
}
