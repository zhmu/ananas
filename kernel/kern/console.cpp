#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/tty.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/lock.h>
#include <ananas/debug-console.h>
#include "options.h"

static spinlock_t spl_consoledrivers = SPINLOCK_DEFAULT_INIT;
static struct CONSOLE_DRIVERS console_drivers;
device_t console_tty = NULL;

#define CONSOLE_BACKLOG_SIZE 1024

static int console_mutex_inuse = 0;
static spinlock_t console_backlog_lock;
static char* console_backlog;
static int console_backlog_pos = 0;
static int console_backlog_overrun = 0;
static mutex_t mtx_console;

/* If set, display the driver list before attaching */
#undef VERBOSE_LIST

static errorcode_t
console_init()
{
	device_t input_dev = NULL;
	device_t output_dev = NULL;

	/*
	 * First of all, we need to sort our console drivers; we do not need to care
	 * about the flags as it only matters that we try them in the correct order.
	 */
	if (!LIST_EMPTY(&console_drivers)) {
		/*
		 * XXX Sorting a linked list sucks as it's hard to find the n-th element; so what
		 * we do is just keep on travering through the list to find the next
		 * element we need to add and add it. This is O(N^2), which is quite bad
		 * (but N will be small, so ...)
		 */
		struct CONSOLE_DRIVERS sort_list;
		LIST_INIT(&sort_list);
		while(!LIST_EMPTY(&console_drivers)) {
			/* Find the element with the highest priority value */
			struct CONSOLE_DRIVER* cur_driver = NULL;
			LIST_FOREACH(&console_drivers, condrv, struct CONSOLE_DRIVER) {
				if (cur_driver != NULL && condrv->con_priority < cur_driver->con_priority)
					continue;
				cur_driver = condrv;
			} 

			/* OK, add the highest priority console at the end of the sorted queue */
			LIST_REMOVE(&console_drivers, cur_driver);
			LIST_APPEND(&sort_list, cur_driver);
		}

#ifdef VERBOSE_LIST
		kprintf("console_init(): driver list\n");
		LIST_FOREACH(&sort_list, condrv, struct CONSOLE_DRIVER) {
			kprintf("%p: '%s' (priority %d, flags 0x%x)\n",
			 condrv->con_driver, condrv->con_driver->name,
			 condrv->con_priority, condrv->con_flags);
		}
#endif

		/*
		 * Now try the sorted list in order.
		 */
		LIST_FOREACH(&sort_list, condrv, struct CONSOLE_DRIVER) {
			/* Skip any driver that is already provided for */
			if (((condrv->con_flags & CONSOLE_FLAG_IN ) == 0 || input_dev  != NULL) &&
			    ((condrv->con_flags & CONSOLE_FLAG_OUT) == 0 || output_dev != NULL))
				continue;

			/*
			 * OK, see if this devices probes. Note that we don't provide any
			 * resources here - XXX devices that require them can't be used as
			 * console devices currently.
			 */
			device_t dev = device_alloc(NULL, condrv->con_driver);
			errorcode_t err = device_attach_single(dev);
			if (ananas_is_failure(err)) {
				/* Too bad; this driver won't work */
				device_free(dev);
				continue;
			}

			/* We have liftoff! */
			if ((condrv->con_flags & CONSOLE_FLAG_IN) && input_dev == NULL)
				input_dev = dev;
			if ((condrv->con_flags & CONSOLE_FLAG_OUT) && output_dev == NULL)
				output_dev = dev;
		}
	}

	if (input_dev != NULL || output_dev != NULL)
		console_tty = tty_alloc(input_dev, output_dev);

	/* Initialize the backlog; we use it to queue messages once the mutex is hold */
	spinlock_init(&console_backlog_lock);
	console_backlog = new char[CONSOLE_BACKLOG_SIZE];
	console_backlog_pos = 0;

	/* Initialize the console print mutex and start using it */
	mutex_init(&mtx_console, "console");
	console_mutex_inuse++;

	return ananas_success();
}

INIT_FUNCTION(console_init, SUBSYSTEM_CONSOLE, ORDER_MIDDLE);

void
console_putchar(int c)
{
#ifdef OPTION_DEBUG_CONSOLE
	debugcon_putch(c);
#endif /* OPTION_DEBUG_CONSOLE */
	if (console_tty == NULL)
		return;
	uint8_t ch = c; // cannot cast due to endianness!
	size_t len = sizeof(ch);
	device_write(console_tty, (const void*)&ch, &len, 0);
}


static void
console_puts(const char* s)
{
	for (int c = *s++; c != 0; c = *s++)
		console_putchar(c);
}

void
console_putstring(const char* s)
{
	if (console_mutex_inuse && !mutex_trylock(&mtx_console)) {
		/*
		 * Couldn't obtain the console mutex; we should put our message in the
		 * back log and wait for it to be picked up later.
		 */
		int len = strlen(s);
		register_t state = spinlock_lock_unpremptible(&console_backlog_lock);
		if (console_backlog_pos + len < CONSOLE_BACKLOG_SIZE) {
			/* Fits! */
			strcpy(&console_backlog[console_backlog_pos], s);
			console_backlog_pos += len;
		} else {
			/* Doesn't fit */
			console_backlog_overrun++;
		}
		spinlock_unlock_unpremptible(&console_backlog_lock, state);
		return;
	}

	console_puts(s);

	/* See if there's anything in the backlog that we can print */
	if (console_backlog_pos > 0) {
		register_t state = spinlock_lock_unpremptible(&console_backlog_lock);
		for (int n = 0; n < console_backlog_pos; n++)
			console_putchar(console_backlog[n]);
		console_backlog_pos = 0;

		if (console_backlog_overrun)
			console_puts("[***OVERRUN***]");
		spinlock_unlock_unpremptible(&console_backlog_lock, state);
	}


	if (console_mutex_inuse)
		mutex_unlock(&mtx_console);
}

uint8_t
console_getchar()
{
#ifdef OPTION_DEBUG_CONSOLE
	int ch = debugcon_getch();
	if (ch != 0)
		return ch;
#endif /* OPTION_DEBUG_CONSOLE */
	if (console_tty == NULL)
		return 0;
	uint8_t c;
	size_t len = sizeof(c);
	if (device_read(console_tty, (char*)&c, &len, 0) < 1)
		return 0;
	return c;
}

errorcode_t
console_register_driver(struct CONSOLE_DRIVER* con)
{
	spinlock_lock(&spl_consoledrivers);
	LIST_APPEND(&console_drivers, con);
	spinlock_unlock(&spl_consoledrivers);
	return ananas_success();
}

errorcode_t
console_unregister_driver(struct CONSOLE_DRIVER* con)
{
	spinlock_lock(&spl_consoledrivers);
	LIST_REMOVE(&console_drivers, con);
	spinlock_unlock(&spl_consoledrivers);
	return ananas_success();
}

/* vim:set ts=2 sw=2: */
