#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/tty.h>
#include <ananas/lib.h>
#include <ananas/debug-console.h>
#include "options.h"

extern const char* config_hints[];
static spinlock_t spl_consoledrivers = SPINLOCK_DEFAULT_INIT;
static struct CONSOLE_DRIVERS console_drivers;
device_t console_tty = NULL;

/* If set, display the driver list before attaching */
#undef VERBOSE_LIST

static errorcode_t
console_init()
{
	char tmphint[32 /* XXX */];
	device_t input_dev = NULL;
	device_t output_dev = NULL;

	/*
	 * First of all, we need to sort our console drivers; we do not need to care
	 * about the flags as it only matters that we try them in the correct order.
	 */
	if (!DQUEUE_EMPTY(&console_drivers)) {
		/*
		 * XXX Sorting a linked list sucks as it's hard to find the n-th element; so what
		 * we do is just keep on travering through the list to find the next
		 * element we need to add and add it. This is O(N^2), which is quite bad
		 * (but N will be small, so ...)
		 */
		struct CONSOLE_DRIVERS sort_list;
		DQUEUE_INIT(&sort_list);
		while(!DQUEUE_EMPTY(&console_drivers)) {
			/* Find the element with the highest priority value */
			struct CONSOLE_DRIVER* cur_driver = NULL;
			DQUEUE_FOREACH(&console_drivers, condrv, struct CONSOLE_DRIVER) {
				if (cur_driver != NULL && condrv->con_priority < cur_driver->con_priority)
					continue;
				cur_driver = condrv;
			} 

			/* OK, add the highest priority console at the end of the sorted queue */
			DQUEUE_REMOVE(&console_drivers, cur_driver);
			DQUEUE_ADD_TAIL(&sort_list, cur_driver);
		}

#ifdef VERBOSE_LIST
		kprintf("console_init(): driver list\n");
		DQUEUE_FOREACH(&sort_list, condrv, struct CONSOLE_DRIVER) {
			kprintf("%p: '%s' (priority %d, flags 0x%x)\n",
			 condrv->con_driver, condrv->con_driver->name,
			 condrv->con_priority, condrv->con_flags);
		}
#endif

		/*
		 * Now try the sorted list in order.
		 */
		DQUEUE_FOREACH(&sort_list, condrv, struct CONSOLE_DRIVER) {
			/* Skip any driver that is already provided for */
			if (((condrv->con_flags & CONSOLE_FLAG_IN ) == 0 || input_dev  != NULL) &&
			    ((condrv->con_flags & CONSOLE_FLAG_OUT) == 0 || output_dev != NULL))
				continue;

			/*
			 * OK, see if this devices probes. To do so, we need to construct a hint -
			 * but as we have no idea on which bus it is, we'll use an asterisk to
			 * match anything.
			 */
			device_t dev = device_alloc(NULL, condrv->con_driver);
			sprintf(tmphint, "*.%s.%u.", dev->name, dev->unit);
			device_get_resources_byhint(dev, tmphint, config_hints);
			errorcode_t err = device_attach_single(dev);
			kprintf("trying %s '%s' -> %u\n", condrv->con_driver->name, tmphint, err);
			if (err != ANANAS_ERROR_OK) {
				/* Too bad; this driver won't work */
				device_free(dev);
				continue;
			}

			/* We have liftoff! */
			if ((condrv->con_flags & CONSOLE_FLAG_IN) && input_dev == NULL)
				input_dev = dev;
			if ((condrv->con_flags & CONSOLE_FLAG_OUT) && output_dev == NULL)
				output_dev = dev;

			kprintf("device %p(%s) probed %p %p\n", dev, dev->name, input_dev, output_dev);
		}
	}

	if (input_dev != NULL || output_dev != NULL)
		console_tty = tty_alloc(input_dev, output_dev);

	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(console_init, SUBSYSTEM_CONSOLE, ORDER_MIDDLE);

void
console_putchar(int c)
{
#ifdef DEBUG_CONSOLE
	debugcon_putch(c);
#endif /* DEBUG_CONSOLE */
	if (console_tty == NULL)
		return;
	uint8_t ch = c; // cannot cast due to endianness!
	size_t len = sizeof(ch);
	device_write(console_tty, (const char*)&ch, &len, 0);
}

uint8_t
console_getchar()
{
#ifdef DEBUG_CONSOLE
	int ch = debugcon_getch();
	if (ch != 0)
		return ch;
#endif /* DEBUG_CONSOLE */
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
	DQUEUE_ADD_TAIL(&console_drivers, con);
	spinlock_unlock(&spl_consoledrivers);
	return ANANAS_ERROR_OK;
}

errorcode_t
console_unregister_driver(struct CONSOLE_DRIVER* con)
{
	spinlock_lock(&spl_consoledrivers);
	DQUEUE_REMOVE(&console_drivers, con);
	spinlock_unlock(&spl_consoledrivers);
	return ANANAS_ERROR_OK;
}

/* vim:set ts=2 sw=2: */
