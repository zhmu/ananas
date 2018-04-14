#include <ananas/types.h>
#include "kernel/dev/tty.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/pcpu.h"
#include "kernel/processgroup.h"
#include "kernel/queue.h"
#include "kernel/result.h"
#include "kernel/signal.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

/* Newline char - cannot be modified using c_cc */
#define NL '\n'
#define CR 0xd

namespace {

void DeliverSignal(int signo)
{
	/*
	 * XXX for now, we'll assume everything needs to go to the thread group of
	 * pid 1. This must not be hardcoded!
	 */
	siginfo_t si{};
	si.si_signo = signo;

	auto pg = process::FindProcessGroupByIDAndLock(1 /* XXX */);
	KASSERT(pg != nullptr, "pgid 1 not found??");
	signal::QueueSignal(*pg, si);
	pg->pg_mutex.Unlock();
}

} // unnamed namespace

TTY::TTY(const Ananas::CreateDeviceProperties& cdp)
	: Device(cdp)
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

Result
TTY::Read(void* buf, size_t& len, off_t offset)
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

		if (tty_in_readpos == tty_in_writepos) {
			/*
			 * Buffer is empty - schedule the thread for a wakeup once we have data.
			  */
			d_Waiters.Wait();
			continue;
		}

		/*
		 * A line is delimited by a newline NL, end-of-file char EOF or end-of-line
		 * EOL char. We will have to scan our input buffer for any of these.
		 */
		int in_len;
		if (tty_in_readpos < tty_in_writepos) {
			in_len = tty_in_writepos - tty_in_readpos;
		} else /* if (tty_in_readpos > tty_in_writepos) */ {
			in_len = (tty_input_queue.size() - tty_in_writepos) + tty_in_readpos;
		}

		/* See if we can find a delimiter here */
#define CHAR_AT(i) (tty_input_queue[(tty_in_readpos + i) % tty_input_queue.size()])
		unsigned int n = 0;
		while (n < in_len) {
			if (CHAR_AT(n) == NL)
				break;
			if (tty_termios.c_cc[VEOF] != _POSIX_VDISABLE && CHAR_AT(n) == tty_termios.c_cc[VEOF])
				break;
			if (tty_termios.c_cc[VEOL] != _POSIX_VDISABLE && CHAR_AT(n) == tty_termios.c_cc[VEOL])
				break;
			n++;
		}
#undef CHAR_AT
		if (n == in_len) {
			/* Line is not complete - try again later */
			d_Waiters.Wait();
			continue;
		}

		/* A full line is available - copy the data over */
		size_t num_read = 0, num_left = len;
		if (num_left > in_len)
			num_left = in_len;
		while (num_left > 0) {
			data[num_read] = tty_input_queue[tty_in_readpos];
			tty_in_readpos = (tty_in_readpos + 1) % tty_input_queue.size();
			num_read++; num_left--;
		}
		len = num_read;
		return Result::Success();
	}

	/* NOTREACHED */
}

void
TTY::PutChar(unsigned char ch)
{
	if (tty_termios.c_oflag & OPOST) {
		if ((tty_termios.c_oflag & ONLCR) && ch == NL) {
			Print("\r", 1);
		}
	}

	Print((const char*)&ch, 1);
}

void
TTY::HandleEcho(unsigned char byte)
{
	if ((tty_termios.c_iflag & ICANON) && (tty_termios.c_oflag & ECHOE) && byte == tty_termios.c_cc[VERASE]) {
		// Need to echo erase char
		char erase_seq[] = { 8, ' ', 8 };
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

Result
TTY::Write(const void* buffer, size_t& len, off_t offset)
{
	size_t left = len;
	for (auto ptr = static_cast<const char*>(buffer); left > 0; ptr++, left--) {
		TTY::HandleEcho(*ptr);
	}
	return Result::Success();
}

Result
TTY::OnInput(const char* buffer, size_t len)
{
	for(/* nothing */; len > 0; buffer++, len--) {
		char ch = *buffer;

		// If we are out of buffer space, just eat the charachter XXX possibly unnecessary for VERASE */
		if ((tty_in_writepos + 1) % tty_input_queue.size() == tty_in_readpos)
			return RESULT_MAKE_FAILURE(ENOSPC);

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

		/* Handle backspace */
		if ((tty_termios.c_iflag & ICANON) && ch == tty_termios.c_cc[VERASE]) {
			if (tty_in_readpos != tty_in_writepos) {
				/* Still a charachter available which wasn't read. Nuke it */
				if (tty_in_writepos > 0)
					tty_in_writepos--;
				else
					tty_in_writepos = tty_input_queue.size() - 1;
			}
		} else if ((tty_termios.c_iflag & ICANON) && ch == tty_termios.c_cc[VKILL]) {
			// Kil the entire line
			while (tty_in_readpos != tty_in_writepos) {
				/* Still a charachter available which wasn't read. Nuke it */
				if (tty_in_writepos > 0)
					tty_in_writepos--;
				else
					tty_in_writepos = tty_input_queue.size() - 1;
				TTY::HandleEcho(8); // XXX will this always work?
			}
		} else {
			/* Store the charachter! */
			tty_input_queue[tty_in_writepos] = ch;
			tty_in_writepos = (tty_in_writepos + 1) % tty_input_queue.size();
		}

		// Handle writing the charachter, if needed
		HandleEcho(ch);

		/* If we have waiters, awaken them */
		d_Waiters.Signal();
	}

	return Result::Success();
}

/* vim:set ts=2 sw=2: */
