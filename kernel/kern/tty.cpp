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
#include <ananas/error.h>
#include <termios.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/queue.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

/* Newline char - cannot be modified using c_cc */
#define NL '\n'
#define CR 0xd

/* Maximum number of bytes in an input queue */
#define MAX_INPUT	256

namespace {

class TTY : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::ICharDeviceOperations
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

	errorcode_t Attach() override;
	errorcode_t Detach() override;

	errorcode_t	Write(const void* data, size_t& len, off_t offset) override;
	errorcode_t Read(void* buf, size_t& len, off_t offset) override;

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

static Thread tty_thread;
static struct TTY_QUEUE tty_queue;
static Semaphore tty_sem;

TTY::TTY(const Ananas::CreateDeviceProperties& cdp)
	: Device(cdp)
{
	/* Use sensible defaults for the termios structure */
	for (int i = 0; i < NCCS; i++)
		tty_termios.c_cc[i] = _POSIX_VDISABLE;
#define CTRL(x) ((x) - 'A' + 1)
	tty_termios.c_cc[VEOF] = CTRL('D');
	tty_termios.c_cc[VKILL] = CTRL('U');
	tty_termios.c_cc[VERASE] = CTRL('H');
#undef CTRL
	tty_termios.c_iflag = ICRNL | ICANON;
	tty_termios.c_oflag = OPOST | ONLCR;
	tty_termios.c_lflag = ECHO | ECHOE;
	tty_termios.c_cflag = CREAD;
}

errorcode_t
TTY::Attach()
{
	// Hook our device to the TTY queue so that we handle it in our thread
	spinlock_lock(tty_queue.tq_lock);
	QUEUE_ADD_TAIL(&tty_queue, this);
	spinlock_unlock(tty_queue.tq_lock);

	return ananas_success();
}

errorcode_t
TTY::Detach()
{
	panic("remove from queue");

	return ananas_success();
}

errorcode_t
TTY::Write(const void* data, size_t& len, off_t offset)
{
	if (tty_output_dev == NULL)
		return ANANAS_ERROR(NO_DEVICE);
	return tty_output_dev->GetCharDeviceOperations()->Write(data, len, offset);
}

errorcode_t
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
			sem_wait(d_Waiters);
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
			sem_wait(d_Waiters);
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
		return ananas_success();
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
TTY::ProcessInput()
{
	/*
	 * This will be called once data is available from our input device; we need
	 * to queue it up.
	 */
	while (1) {
		KASSERT(tty_input_dev != NULL, "woke up without input device?");

		unsigned char byte;
		size_t len = sizeof(byte);
		errorcode_t err = tty_input_dev->GetCharDeviceOperations()->Read(static_cast<void*>(&byte), len, 0);
		if (ananas_is_failure(err) || len == 0)
			break;

		/* If we are out of buffer space, just eat the charachter XXX possibly unnecessary for VERASE */
		if ((tty_in_writepos + 1) % MAX_INPUT == tty_in_readpos)
			continue;

		/* Handle CR/NL transformations */
		if ((tty_termios.c_iflag & INLCR) && byte == NL)
			byte = CR;
		else if ((tty_termios.c_iflag & IGNCR) && byte == CR)
			continue;
		else if ((tty_termios.c_iflag & ICRNL) && byte == CR)
			byte = NL;

		/* Handle backspace */
		if ((tty_termios.c_iflag & ICANON) && byte == tty_termios.c_cc[VERASE]) {
			if (tty_in_readpos != tty_in_writepos) {
				/* Still a charachter available which wasn't read. Nuke it */
				if (tty_in_writepos > 0)
					tty_in_writepos--;
				else
					tty_in_writepos = MAX_INPUT - 1;
			}
		} else {
			/* Store the charachter! */
			tty_input_queue[tty_in_writepos] = byte;
			tty_in_writepos = (tty_in_writepos + 1) % MAX_INPUT;
		}

		/* Handle writing the charachter, if needed (and we can do so) */
		if (tty_output_dev != NULL)
			HandleEcho(byte);
	}

	/* If we have waiters, awaken them */
	sem_signal(d_Waiters);
}

static void
tty_thread_func(void* ptr)
{
	while(1) {
		sem_wait(tty_sem);

		spinlock_lock(tty_queue.tq_lock);
		KASSERT(!QUEUE_EMPTY(&tty_queue), "woken up without tty's?");
		QUEUE_FOREACH(&tty_queue, tty, TTY) {
			tty->ProcessInput();
		}
		spinlock_unlock(tty_queue.tq_lock);
	}
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
		if (ananas_is_failure(tty->Attach()))
			panic("tty::Attach() failed");
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

void
tty_signal_data()
{
	sem_signal(tty_sem);
}

static errorcode_t
tty_preinit()
{
	/* Initialize the queue of all tty's */
	QUEUE_INIT(&tty_queue);
	spinlock_init(tty_queue.tq_lock);
	sem_init(tty_sem, 0);
	return ananas_success();
}

INIT_FUNCTION(tty_preinit, SUBSYSTEM_CONSOLE, ORDER_SECOND);

static errorcode_t
tty_init()
{
	/* Launch our kernel thread */
	kthread_init(tty_thread, "tty", tty_thread_func, NULL);
	thread_resume(tty_thread);
	return ananas_success();
}

INIT_FUNCTION(tty_init, SUBSYSTEM_TTY, ORDER_ANY);

/* vim:set ts=2 sw=2: */
