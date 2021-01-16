/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <termios.h>
#include "kernel/device.h"
#include "kernel/lock.h"
#include <array>

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

    Result Read(void* buf, size_t len, off_t offset) override;
    Result Write(const void* buffer, size_t len, off_t offset) override;

    Result OnInput(const char* buffer, size_t len);

  protected:
    virtual Result Print(const char* buffer, size_t len) = 0;

  private:
    void DeliverSignal(int signo);
    void SetForegroundProcessGroup(process::ProcessGroup* pg);

    static constexpr size_t InputQueueSize = 256;

    void PutChar(unsigned char ch);
    void HandleEcho(unsigned char byte);

    struct termios tty_termios;
    std::array<char, InputQueueSize> tty_input_queue;
    unsigned int tty_in_writepos = 0;
    unsigned int tty_in_readpos = 0;

    process::Session* tty_session = nullptr;            // session we belong to
    process::ProcessGroup* tty_foreground_pg = nullptr; // foreground process group
    Semaphore tty_waiters{"tty", 1};
};
