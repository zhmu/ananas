/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/flags.h>
#include <ananas/tty.h>
#include <ananas/termios.h>
#include "kernel/dev/tty.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/pcpu.h"
#include "kernel/processgroup.h"
#include "kernel/queue.h"
#include "kernel/result.h"
#include "kernel/signal.h"
#include "kernel/thread.h"
#include "kernel/vfs/types.h"

/* Newline char - cannot be modified using c_cc */
#define NL '\n'
#define CR 0xd

TTY::TTY(const CreateDeviceProperties& cdp) : Device(cdp)
{
    /* Use sensible defaults for the termios structure */
    for (int i = 0; i < NCCS; i++)
        tty_termios.c_cc[i] = _POSIX_VDISABLE;
#define CTRL(x) ((x) - 'A' + 1)
    tty_termios.c_cc[VINTR] = CTRL('C');
    tty_termios.c_cc[VEOF] = CTRL('D');
    tty_termios.c_cc[VKILL] = CTRL('U');
    tty_termios.c_cc[VERASE] = CTRL('H');
    tty_termios.c_cc[VSUSP] = CTRL('Z');
#undef CTRL
    tty_termios.c_iflag = ICRNL | ICANON;
    tty_termios.c_oflag = OPOST | ONLCR;
    tty_termios.c_lflag = ECHO | ECHOE | ISIG;
    tty_termios.c_cflag = CREAD;
}

TTY::~TTY() { SetForegroundProcessGroup(nullptr); }

Result TTY::Read(VFS_FILE& file, void* buf, size_t len)
{
    auto data = static_cast<char*>(buf);

    /*
     * We must read from a tty. XXX We assume blocking does not apply.
     */
    while (1) {
        if ((tty_termios.c_iflag & ICANON) == 0) {
            /* Canonical input is off - handle requests immediately */
            panic("XXX implement me: icanon off!");
        }

        if (tty_input_queue.empty()) {
            /*
             * Buffer is empty - schedule the thread for a wakeup once we have data.
             */
            if (file.f_flags & O_NONBLOCK) return Result::Failure(EAGAIN);
            tty_waiters.Wait();
            continue;
        }

        /*
         * A line is delimited by a newline NL, end-of-file char EOF or end-of-line
         * EOL char. We will have to scan our input buffer for any of these.
         */
        const auto in_len = tty_input_queue.size();
        size_t n = 0;
        for(const auto ch: tty_input_queue) {
            if (ch == NL) break;
            if (tty_termios.c_cc[VEOF] != _POSIX_VDISABLE && ch == tty_termios.c_cc[VEOF])
                break;
            if (tty_termios.c_cc[VEOL] != _POSIX_VDISABLE && ch == tty_termios.c_cc[VEOL])
                break;
            ++n;
        }
        if (n == in_len) {
            /* Line is not complete - try again later */
            tty_waiters.Wait();
            continue;
        }

        /* A full line is available - copy the data over */
        size_t num_read = 0, num_left = len;
        if (num_left > in_len)
            num_left = in_len;
        while (num_left > 0) {
            data[num_read] = tty_input_queue.front();
            tty_input_queue.pop_front();
            ++num_read, --num_left;
        }
        return Result::Success(num_read);
    }

    /* NOTREACHED */
}

void TTY::PutChar(unsigned char ch)
{
    if (tty_termios.c_oflag & OPOST) {
        if ((tty_termios.c_oflag & ONLCR) && ch == NL) {
            Print("\r", 1);
        }
    }

    Print((const char*)&ch, 1);
}

void TTY::HandleEcho(unsigned char byte)
{
    if ((tty_termios.c_iflag & ICANON) && (tty_termios.c_oflag & ECHOE) &&
        byte == tty_termios.c_cc[VERASE]) {
        // Need to echo erase char
        char erase_seq[] = {8, ' ', 8};
        size_t erase_len = sizeof(erase_seq);
        Print(erase_seq, erase_len);
        return;
    }
    if ((tty_termios.c_lflag & (ICANON | ECHONL)) && byte == NL) {
        PutChar(byte);
        return;
    }
    if (tty_termios.c_lflag & ECHO)
        PutChar(byte);
}

Result TTY::Write(VFS_FILE& file, const void* buffer, size_t len)
{
    size_t left = len;
    for (auto ptr = static_cast<const char*>(buffer); left > 0; ptr++, left--) {
        TTY::HandleEcho(*ptr);
    }
    return Result::Success(len);
}

Result TTY::OnInput(const char* buffer, size_t len)
{
    for (/* nothing */; len > 0; buffer++, len--) {
        char ch = *buffer;

        // If we are out of buffer space, just eat the charachter XXX possibly unnecessary for
        // VERASE */
        if (tty_input_queue.full())
            return Result::Failure(ENOSPC);

        /* Handle CR/NL transformations */
        if ((tty_termios.c_iflag & INLCR) && ch == NL)
            ch = CR;
        else if ((tty_termios.c_iflag & IGNCR) && ch == CR)
            continue;
        else if ((tty_termios.c_iflag & ICRNL) && ch == CR)
            ch = NL;

        /* Handle things that need to raise signals */
        if (tty_termios.c_lflag & ISIG) {
            if (ch == tty_termios.c_cc[VINTR]) {
                DeliverSignal(SIGINT);
                continue;
            }
            if (ch == tty_termios.c_cc[VQUIT]) {
                DeliverSignal(SIGQUIT);
                continue;
            }
            if (ch == tty_termios.c_cc[VSUSP]) {
                DeliverSignal(SIGTSTP);
                continue;
            }
        }

        const auto canon = (tty_termios.c_iflag & ICANON) != 0;
        if (canon && ch == tty_termios.c_cc[VERASE]) {
            // Backspace
            if (!tty_input_queue.empty())
                tty_input_queue.pop_back();
        } else if (canon && ch == tty_termios.c_cc[VKILL]) {
            // Kill; echo a backspace per character
            while (!tty_input_queue.empty()) {
                tty_input_queue.pop_front();
                TTY::HandleEcho(8); // XXX will this always work?
            }
        } else {
            // Store
            tty_input_queue.push_back(ch);
        }

        // Handle writing the charachter, if needed
        HandleEcho(ch);

        /* If we have waiters, awaken them */
        tty_waiters.Signal();
    }

    return Result::Success();
}

Result TTY::Open(Process* p) { return Result::Success(); }

Result TTY::Close(Process* p)
{
    // XXX how do we handle us disappearing?
    return Result::Success();
}

Result TTY::IOControl(Process* proc, unsigned long req, void* buffer[])
{
    KASSERT(proc->p_group != nullptr, "no group");
    auto& session = proc->p_group->pg_session;

    switch (req) {
        case TIOCSCTTY: { // Set controlling tty
            if (tty_session != nullptr && tty_session != &session) {
                // TTY already belongs to a different session; reject
                return Result::Failure(EPERM);
            }
            tty_session = &session; // XXX locking
            SetForegroundProcessGroup(proc->p_group);

            {
                MutexGuard g(session.s_mutex);
                session.s_control_tty = this; // XXX how do we handle us disappearing?
            }
            return Result::Success();
        }
        case TIOCSPGRP: { // Set foreground process group
            auto pgid = reinterpret_cast<pid_t>(buffer[0]);
            auto pg = process::FindProcessGroupByID(pgid);
            if (!pg)
                return Result::Failure(EPERM);
            if (&pg->pg_session != &session) {
                pg.Unlock();
                return Result::Failure(EPERM); // process group outside our session
            }

            // TODO are we the ctty?
            SetForegroundProcessGroup(&*pg);
            pg.Unlock();
            return Result::Success();
        }
        case TIOCGPGRP: { // Get foreground process group
            if (tty_foreground_pg != nullptr)
                return Result::Success(tty_foreground_pg->pg_id);
            // XXX Otherwise, return some value >=1 that is not an existing process group ID
            return Result::Failure(ENOTTY);
        }
        case TIOGDNAME: { // Get device name
            const auto p = static_cast<char*>(buffer[0]);
            const auto len = reinterpret_cast<size_t>(buffer[1]);
            const auto name_len = strlen(d_Name) + 1 /* \0 */;
            if (name_len > len) return Result::Failure(ERANGE);
            memcpy(p, d_Name, name_len);
            return Result::Success();
        }
        case TIOCGETA: { // Get terminal attributes
            auto p = static_cast<struct termios*>(buffer[0]);
            memcpy(p, &tty_termios, sizeof(struct termios));
            return Result::Success();
        }
        case TIOCSETA:
        case TIOCSETW:
        case TIOCSETWF: { // Set terminal attributes
            auto p = static_cast<const struct termios*>(buffer[0]);
            if (auto result = OnTerminalAttributes(*p); result.IsFailure())
                return result;
            memcpy(&tty_termios, p, sizeof(struct termios));
            return Result::Success();
        }
    }

    return Result::Failure(EINVAL);
}

void TTY::DeliverSignal(int signo)
{
    if (tty_foreground_pg == nullptr)
        return;

    siginfo_t si{};
    si.si_signo = signo;
    tty_foreground_pg->Lock();
    signal::SendSignal(*tty_foreground_pg, si);
    tty_foreground_pg->Unlock();
}

void TTY::SetForegroundProcessGroup(process::ProcessGroup* pg)
{
    if (tty_foreground_pg != nullptr)
        tty_foreground_pg->RemoveReference();
    if (pg != nullptr)
        pg->AddReference();
    tty_foreground_pg = pg;
}

Result TTY::OnTerminalAttributes(const struct termios& tios)
{
    // TODO: check if these are valid
    return Result::Success();
}

bool TTY::CanRead()
{
    return !tty_input_queue.empty();
}
