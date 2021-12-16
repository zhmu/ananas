/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/util/ring_buffer.h>
#include <ananas/termios.h>
#include "kernel/condvar.h"
#include "kernel/device.h"
#include "kernel/lock.h"

namespace process
{
    struct ProcessGroup;
    struct Session;
} // namespace process

class TTY : public Device, private IDeviceOperations, private ICharDeviceOperations
{
  public:
    TTY(const CreateDeviceProperties& cdp);
    virtual ~TTY();

    IDeviceOperations& GetDeviceOperations() override { return *this; }

    ICharDeviceOperations* GetCharDeviceOperations() override { return this; }

    Result Open(Process* p) override;
    Result Close(Process* p) override;
    Result IOControl(Process* proc, unsigned long req, void* buffer[]) override;
    using IDeviceOperations::DetermineDevicePhysicalAddres;

    Result Read(VFS_FILE& file, void* buf, size_t len) override;
    Result Write(VFS_FILE& file, const void* buffer, size_t len) override;
    bool CanRead() override;

    Result OnInput(const char* buffer, size_t len);
    virtual Result OnTerminalAttributes(const struct termios&);

  protected:
    virtual Result Print(const char* buffer, size_t len) = 0;

  private:
    void DeliverSignal(int signo);
    void SetForegroundProcessGroup(process::ProcessGroup* pg);

    static constexpr size_t InputQueueSize = 256;

    void PutChar(unsigned char ch);
    void HandleEcho(unsigned char byte);

    struct termios tty_termios;
    util::ring_buffer<char, InputQueueSize> tty_input_queue;

    process::Session* tty_session = nullptr;            // session we belong to
    process::ProcessGroup* tty_foreground_pg = nullptr; // foreground process group
    Mutex tty_mutex{"tty"};
    ConditionVariable tty_cv{"ttywait"};
};
