/*
 * Implementation of our TTY device; this multiplexes input/output devices to a
 * single device, and handles the TTY magic.
 *
 * Actual processing of data is handled by the 'tty' kernel thread; this is done
 * because we cannot do so from interrupt context, which is typically the point
 * where data enters the TTY device. Instead of creating one thread per device,
 * we spawn a generic thread which takes care of all of them.
 */
#include <ananas/types.h>
#include <termios.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/pcpu.h"
#include "kernel/processgroup.h"
#include "kernel/queue.h"
#include "kernel/result.h"
#include "kernel/signal.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/dev/kbdmux.h"

TRACE_SETUP;

/* Newline char - cannot be modified using c_cc */
#define NL '\n'
#define CR 0xd

/* Maximum number of bytes in an input queue */
#define MAX_INPUT	256

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

class TTY : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::ICharDeviceOperations, private keyboard_mux::IKeyboardConsumer
{
public:
	TTY(const Ananas::CreateDeviceProperties& cdp);
	virtual ~TTY() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	ICharDeviceOperations* GetCharDeviceOperations() override
	{
		return this;
	}

	Result Attach() override;
	Result Detach() override;

	Result Write(const void* data, size_t& len, off_t offset) override;
	Result Read(void* buf, size_t& len, off_t offset) override;

	void OnCharacter(int ch) override;

	/*
	 * This is crude, but we'll need a queue for all TTY devices so that
	 * we can handle data for them one by one.
	 */
	QUEUE_FIELDS(TTY);
	void ProcessInput();

	void SetInputDevice(Ananas::Device* input_dev)
	{
		tty_input_dev = input_dev;
	}

	void SetOutputDevice(Ananas::Device* output_dev)
	{
		tty_output_dev = output_dev;
	}

	Ananas::Device* tty_input_dev = nullptr;
	Ananas::Device* tty_output_dev = nullptr;

private:
	void PutChar(unsigned char ch);
	void HandleEcho(unsigned char byte);

	struct termios	tty_termios;
	char						tty_input_queue[MAX_INPUT];
	unsigned int		tty_in_writepos = 0;
	unsigned int		tty_in_readpos = 0;
};

QUEUE_DEFINE_BEGIN(TTY_QUEUE, TTY)
	Spinlock tq_lock;
QUEUE_DEFINE_END

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
TTY::Attach()
{
	keyboard_mux::RegisterConsumer(*this);
	return Result::Success();
}

Result
TTY::Detach()
{
	keyboard_mux::UnregisterConsumer(*this);
	return Result::Success();
}

Result
TTY::Write(const void* data, size_t& len, off_t offset)
{
	if (tty_output_dev == NULL)
		return RESULT_MAKE_FAILURE(ENODEV);
	return tty_output_dev->GetCharDeviceOperations()->Write(data, len, offset);
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
			in_len = (MAX_INPUT - tty_in_writepos) + tty_in_readpos;
		}

		/* See if we can find a delimiter here */
#define CHAR_AT(i) (tty_input_queue[(tty_in_readpos + i) % MAX_INPUT])
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
			tty_in_readpos = (tty_in_readpos + 1) % MAX_INPUT;
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
	size_t len = 1;
	if (tty_termios.c_oflag & OPOST) {
		if ((tty_termios.c_oflag & ONLCR) && ch == NL) {
			tty_output_dev->GetCharDeviceOperations()->Write("\r", len, 0);
			len = 1;
		}
	}

	tty_output_dev->GetCharDeviceOperations()->Write((const char*)&ch, len, 0);
}

void
TTY::HandleEcho(unsigned char byte)
{
	if ((tty_termios.c_iflag & ICANON) && (tty_termios.c_oflag & ECHOE) && byte == tty_termios.c_cc[VERASE]) {
		/* Need to echo erase char */
		char erase_seq[] = { 8, ' ', 8 };
		size_t erase_len = sizeof(erase_seq);
		tty_output_dev->GetCharDeviceOperations()->Write(erase_seq, erase_len, 0);
		return;
	}
	if ((tty_termios.c_lflag & (ICANON | ECHONL)) && byte == NL) {
		PutChar(byte);
		return;
	}
	if (tty_termios.c_lflag & ECHO)
		PutChar(byte);
}

void
TTY::OnCharacter(int ch)
{
	// If we are out of buffer space, just eat the charachter XXX possibly unnecessary for VERASE */
	if ((tty_in_writepos + 1) % MAX_INPUT == tty_in_readpos)
		return;

	/* Handle CR/NL transformations */
	if ((tty_termios.c_iflag & INLCR) && ch == NL)
		ch = CR;
	else if ((tty_termios.c_iflag & IGNCR) && ch == CR)
		return;
	else if ((tty_termios.c_iflag & ICRNL) && ch == CR)
		ch = NL;

	/* Handle things that need to raise signals */
	if (tty_termios.c_lflag & ISIG) {
		if (ch == tty_termios.c_cc[VINTR]) {
			DeliverSignal(SIGINT);
			return;
		}
		if (ch == tty_termios.c_cc[VQUIT]) {
			DeliverSignal(SIGQUIT);
			return;
		}
		if (ch == tty_termios.c_cc[VSUSP]) {
			DeliverSignal(SIGTSTP);
			return;
		}
	}

	/* Handle backspace */
	if ((tty_termios.c_iflag & ICANON) && ch == tty_termios.c_cc[VERASE]) {
		if (tty_in_readpos != tty_in_writepos) {
			/* Still a charachter available which wasn't read. Nuke it */
			if (tty_in_writepos > 0)
				tty_in_writepos--;
			else
				tty_in_writepos = MAX_INPUT - 1;
		}
	} else if ((tty_termios.c_iflag & ICANON) && ch == tty_termios.c_cc[VKILL]) {
		// Kil the entire line
		while (tty_in_readpos != tty_in_writepos) {
			/* Still a charachter available which wasn't read. Nuke it */
			if (tty_in_writepos > 0)
				tty_in_writepos--;
			else
				tty_in_writepos = MAX_INPUT - 1;
			TTY::HandleEcho(8); // XXX will this always work?
		}
	} else {
		/* Store the charachter! */
		tty_input_queue[tty_in_writepos] = ch;
		tty_in_writepos = (tty_in_writepos + 1) % MAX_INPUT;
	}

	/* Handle writing the charachter, if needed (and we can do so) */
	if (tty_output_dev != NULL)
		HandleEcho(ch);

	/* If we have waiters, awaken them */
	d_Waiters.Signal();
}

struct TTY_Driver : public Ananas::Driver
{
	TTY_Driver()
	 : Driver("tty")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return nullptr; // instantiated by tty_alloc()
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return new TTY(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(TTY_Driver)

Ananas::Device*
tty_alloc(Ananas::Device* input_dev, Ananas::Device* output_dev)
{
	auto tty = static_cast<TTY*>(Ananas::DeviceManager::CreateDevice("tty", Ananas::CreateDeviceProperties(Ananas::ResourceSet())));
	if (tty != nullptr) {
		tty->SetInputDevice(input_dev);
		tty->SetOutputDevice(output_dev);
		if (auto result = tty->Attach(); result.IsFailure())
			panic("tty::Attach() failed, %d", result.AsStatusCode());
	}
	return tty;
}

Ananas::Device*
tty_get_inputdev(Ananas::Device* dev)
{
	auto tty = reinterpret_cast<TTY*>(dev);
	return tty != nullptr ? tty->tty_input_dev : nullptr;
}

Ananas::Device*
tty_get_outputdev(Ananas::Device* dev)
{
	auto tty = reinterpret_cast<TTY*>(dev);
	return tty != nullptr ? tty->tty_output_dev : nullptr;
}

/* vim:set ts=2 sw=2: */
