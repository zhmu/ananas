#include "console.h"
#include "device.h"
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
	device_get_resources(console_dev, config_hints);
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
