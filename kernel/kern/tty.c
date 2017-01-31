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
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/queue.h>
#include <ananas/limits.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <termios.h>

TRACE_SETUP;

/* Newline char - cannot be modified using c_cc */
#define NL '\n'
#define CR 0xd

struct TTY_PRIVDATA {
	device_t				device;
	device_t				input_dev;
	device_t				output_dev;

	struct termios	termios;
	char						input_queue[MAX_INPUT];
	unsigned int		in_writepos;
	unsigned int		in_readpos;

	/*
	 * This is crude, but we'll need a queue for all TTY devices so that
	 * we can handle data for them one by one.
	 */
	QUEUE_FIELDS(struct TTY_PRIVDATA);
};

QUEUE_DEFINE_BEGIN(TTY_QUEUE, struct TTY_PRIVDATA)
	spinlock_t tq_lock;
QUEUE_DEFINE_END

static struct DRIVER drv_tty;
static thread_t tty_thread;
static struct TTY_QUEUE tty_queue;
static semaphore_t tty_sem;

device_t
tty_alloc(device_t input_dev, device_t output_dev)
{
	device_t dev = device_alloc(NULL, &drv_tty);
	if (dev == NULL)
		return NULL;

	struct TTY_PRIVDATA* priv = kmalloc(sizeof(struct TTY_PRIVDATA));
	memset(priv, 0, sizeof(struct TTY_PRIVDATA));
	priv->input_dev = input_dev;
	priv->output_dev = output_dev;
	priv->device = dev; /* backref for kernel thread */
	dev->privdata = priv;

	/* Use sensible defaults for the termios structure */
	for (int i = 0; i < NCCS; i++)
		priv->termios.c_cc[i] = _POSIX_VDISABLE;
#define CTRL(x) ((x) - 'A' + 1)
	priv->termios.c_cc[VEOF] = CTRL('D');
	priv->termios.c_cc[VKILL] = CTRL('U');
	priv->termios.c_cc[VERASE] = CTRL('H');
#undef CTRL
	priv->termios.c_iflag = ICRNL | ICANON;
	priv->termios.c_oflag = OPOST | ONLCR;
	priv->termios.c_lflag = ECHO | ECHOE;
	priv->termios.c_cflag = CREAD;

	/* Hook our device to the TTY queue so that we handle it in our thread */
	spinlock_lock(&tty_queue.tq_lock);
	QUEUE_ADD_TAIL(&tty_queue, priv);
	spinlock_unlock(&tty_queue.tq_lock);
	return dev;
}

device_t
tty_get_inputdev(device_t dev)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;
	return priv->input_dev;
}

device_t
tty_get_outputdev(device_t dev)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;
	return priv->output_dev;
}

static errorcode_t
tty_write(device_t dev, const void* data, size_t* len, off_t offset)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;
	if (priv->output_dev == NULL)
		return ANANAS_ERROR(NO_DEVICE);
	return device_write(priv->output_dev, data, len, offset);
}

static errorcode_t
tty_read(device_t dev, void* buf, size_t* len, off_t offset)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;
	char* data = (char*)buf;

	/*
	 * We must read from a tty. XXX We assume blocking does not apply.
	 */
	while (1) {
		if ((priv->termios.c_iflag & ICANON) == 0) {
			/* Canonical input is off - handle requests immediately */
			panic("XXX implement me: icanon off!");
		}

		if (priv->in_readpos == priv->in_writepos) {
			/*
			 * Buffer is empty - schedule the thread for a wakeup once we have data.
			  */
			sem_wait(&dev->waiters);
			continue;
		}

		/*
		 * A line is delimited by a newline NL, end-of-file char EOF or end-of-line
		 * EOL char. We will have to scan our input buffer for any of these.
		 */
		int in_len;
		if (priv->in_readpos < priv->in_writepos) {
			in_len = priv->in_writepos - priv->in_readpos;
		} else /* if (priv->in_readpos > priv->in_writepos) */ {
			in_len = (MAX_INPUT - priv->in_writepos) + priv->in_readpos;
		}

		/* See if we can find a delimiter here */
#define CHAR_AT(i) (priv->input_queue[(priv->in_readpos + i) % MAX_INPUT])
		unsigned int n = 0;
		while (n < in_len) {
			if (CHAR_AT(n) == NL)
				break;
			if (priv->termios.c_cc[VEOF] != _POSIX_VDISABLE && CHAR_AT(n) == priv->termios.c_cc[VEOF])
				break;
			if (priv->termios.c_cc[VEOL] != _POSIX_VDISABLE && CHAR_AT(n) == priv->termios.c_cc[VEOL])
				break;
			n++;
		}
#undef CHAR_AT
		if (n == in_len) {
			/* Line is not complete - try again later */
			sem_wait(&dev->waiters);
			continue;
		}

		/* A full line is available - copy the data over */
		size_t num_read = 0, num_left = *len;
		if (num_left > in_len)
			num_left = in_len;
		while (num_left > 0) {
			data[num_read] = priv->input_queue[priv->in_readpos];
			priv->in_readpos = (priv->in_readpos + 1) % MAX_INPUT;
			num_read++; num_left--;
		}
		*len = num_read;
		return ANANAS_ERROR_NONE;
	}

	/* NOTREACHED */
}

static void
tty_putchar(device_t dev, unsigned char ch)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;

	size_t len = 1;
	if (priv->termios.c_oflag & OPOST) {
		if ((priv->termios.c_oflag & ONLCR) && ch == NL) {
			device_write(priv->output_dev, "\r", &len, 0);
			len = 1;
		}
	}
	device_write(priv->output_dev, (const char*)&ch, &len, 0);
}

static void
tty_handle_echo(device_t dev, unsigned char byte)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;
	if ((priv->termios.c_iflag & ICANON) && (priv->termios.c_oflag & ECHOE) && byte == priv->termios.c_cc[VERASE]) {
		/* Need to echo erase char */
		char erase_seq[] = { 8, ' ', 8 };
		size_t erase_len = sizeof(erase_seq);
		device_write(priv->output_dev, erase_seq, &erase_len, 0);
		return;
	}
	if ((priv->termios.c_lflag & (ICANON | ECHONL)) && byte == NL) {
		tty_putchar(dev, byte);
		return;
	}
	if (priv->termios.c_lflag & ECHO)
		tty_putchar(dev, byte);
}

static void
tty_handle_input(device_t dev)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;

	/*
	 * This will be called once data is available from our input device; we need
	 * to queue it up.
	 */
	while (1) {
		KASSERT(priv->input_dev != NULL, "woke up without input device?");

		unsigned char byte;
		size_t len = sizeof(byte);
		errorcode_t err = device_read(priv->input_dev, (char*)&byte, &len, 0);
		if (err != ANANAS_ERROR_NONE || len == 0)
			break;

		/* If we are out of buffer space, just eat the charachter XXX possibly unnecessary for VERASE */
		if ((priv->in_writepos + 1) % MAX_INPUT == priv->in_readpos)
			continue;

		/* Handle CR/NL transformations */
		if ((priv->termios.c_iflag & INLCR) && byte == NL)
			byte = CR;
		else if ((priv->termios.c_iflag & IGNCR) && byte == CR)
			continue;
		else if ((priv->termios.c_iflag & ICRNL) && byte == CR)
			byte = NL;

		/* Handle backspace */
		if ((priv->termios.c_iflag & ICANON) && byte == priv->termios.c_cc[VERASE]) {
			if (priv->in_readpos != priv->in_writepos) {
				/* Still a charachter available which wasn't read. Nuke it */
				if (priv->in_writepos > 0)
					priv->in_writepos--;
				else
					priv->in_writepos = MAX_INPUT - 1;
			}
		} else {
			/* Store the charachter! */
			priv->input_queue[priv->in_writepos] = byte;
			priv->in_writepos = (priv->in_writepos + 1) % MAX_INPUT;
		}

		/* Handle writing the charachter, if needed (and we can do so) */
		if (priv->output_dev != NULL)
			tty_handle_echo(dev, byte);
	}

	/* If we have waiters, awaken them */
	sem_signal(&dev->waiters);
}

static void
tty_thread_func(void* ptr)
{
	while(1) {
		sem_wait(&tty_sem);

		spinlock_lock(&tty_queue.tq_lock);
		KASSERT(!QUEUE_EMPTY(&tty_queue), "woken up without tty's?");
		QUEUE_FOREACH(&tty_queue, priv, struct TTY_PRIVDATA) {
			tty_handle_input(priv->device);
		}
		spinlock_unlock(&tty_queue.tq_lock);
	}
}

void
tty_signal_data()
{
	sem_signal(&tty_sem);
}

static errorcode_t
tty_preinit()
{
	/* Initialize the queue of all tty's */
	QUEUE_INIT(&tty_queue);
	spinlock_init(&tty_queue.tq_lock);
	sem_init(&tty_sem, 0);
	return ANANAS_ERROR_NONE;
}

INIT_FUNCTION(tty_preinit, SUBSYSTEM_CONSOLE, ORDER_SECOND);

static errorcode_t
tty_init()
{
	/* Launch our kernel thread */
	kthread_init(&tty_thread, "tty", tty_thread_func, NULL);
	thread_resume(&tty_thread);
	return ANANAS_ERROR_NONE;
}

INIT_FUNCTION(tty_init, SUBSYSTEM_TTY, ORDER_ANY);

static struct DRIVER drv_tty = {
	.name					= "tty",
	.drv_probe		= NULL,
	.drv_read			= tty_read,
	.drv_write		= tty_write
};

/* vim:set ts=2 sw=2: */
