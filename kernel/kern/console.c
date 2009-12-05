#include "console.h"
#include "device.h"
#include "lib.h"
#include "console.inc"

device_t input_dev = NULL;
device_t output_dev = NULL;
extern const char* config_hints[];

void
console_init()
{
	char tmphint[32 /* XXX */];

	/*
	 * The console is magic; it consists of two parts: input/output, and it's
	 * about the first thing we initialize (which makes sense, as we want to have
	 * a means to show the user what's going on as early as possible).
	 */
#ifdef CONSOLE_INPUT_DRIVER
	extern struct DRIVER CONSOLE_INPUT_DRIVER;
	input_dev = device_alloc(NULL, &CONSOLE_INPUT_DRIVER);
	/*
	 * Cheat: we have no way to figure out what the eventual bus is the
	 * console driver will be attached to, so we allow this to be hardcoded
	 * in the configuration as well.
	 */
#ifdef CONSOLE_INPUT_BUS
	sprintf(tmphint, "%s.%s.%u.", CONSOLE_INPUT_BUS, input_dev->name, input_dev->unit);
#else
	sprintf(tmphint, "%s.%u.", input_dev->name, input_dev->unit);
#endif
	device_get_resources_byhint(input_dev, tmphint, config_hints);
	device_attach_single(input_dev);
#endif /* CONSOLE_INPUT_DRIVER */

#ifdef CONSOLE_OUTPUT_DRIVER
	extern struct DRIVER CONSOLE_OUTPUT_DRIVER;
	output_dev = device_alloc(NULL, &CONSOLE_OUTPUT_DRIVER);
#ifdef CONSOLE_OUTPUT_BUS
	sprintf(tmphint, "%s.%s.%u.", CONSOLE_OUTPUT_BUS, output_dev->name, output_dev->unit);
#else
	sprintf(tmphint, "%s.%u.", output_dev->name, output_dev->unit);
#endif
	device_get_resources_byhint(output_dev, tmphint, config_hints);
	device_attach_single(output_dev);
#endif /* CONSOLE_DRIVER */
}

void
console_putchar(int c)
{
#if 0
	/* HACK */
	outb(0xe9, c);
	return;
#endif

	if (output_dev == NULL)
		return;

	output_dev->driver->drv_write(output_dev, (const char*)&c, 1);
}

uint8_t
console_getchar()
{
	if (input_dev == NULL)
		return 0;
	uint8_t c;
	if (input_dev->driver->drv_read(input_dev, (const char*)&c, 1) < 1)
		return 0;
	return c;
}

/* vim:set ts=2 sw=2: */
