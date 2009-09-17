#include "console.h"
#include "device.h"
#include "lib.h"
#include "console.inc"

device_t console_dev = NULL;
extern const char* config_hints[];

void
console_init()
{
	/*
	 * The console is magic; it's about the first thing we initialize (which
	 * makes sense, as we want to have a means to show the user what's going on
	 * as early as possible).
	 */
#ifdef CONSOLE_DRIVER
	extern struct DRIVER CONSOLE_DRIVER;
	console_dev = device_alloc(NULL, &CONSOLE_DRIVER);
	/*
	 * Cheat: we have no way to figure out what the eventual bus is the
	 * console driver will be attached to, so we allow this to be hardcoded
	 * in the configuration as well.
	 */
	char tmphint[32 /* XXX */];
#ifdef CONSOLE_BUS
	sprintf(tmphint, "%s.%s.%u.", CONSOLE_BUS, console_dev->name, console_dev->unit);
#else
	sprintf(tmphint, "%s.%u.", console_dev->name, console_dev->unit);
#endif
	device_get_resources_byhint(console_dev, tmphint, config_hints);
	device_attach_single(console_dev);
#endif /* CONSOLE_DRIVER */
}

void
console_putchar(int c)
{
	if (console_dev == NULL)
		return;

	console_dev->driver->drv_write(console_dev, (const char*)&c, 1);
}

/* vim:set ts=2 sw=2: */
