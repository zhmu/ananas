#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/tty.h>
#include <ananas/lib.h>
#include <ananas/debug-console.h>
#include "console.inc"
#include "options.h"

extern const char* config_hints[];
device_t console_tty = NULL;

void
console_init()
{
	char tmphint[32 /* XXX */];
	device_t input_dev = NULL;
	device_t output_dev = NULL;

	/*
	 * The console is magic; it consists of two parts: input/output, and it's
	 * about the first thing we initialize (which makes sense, as we want to have
	 * a means to show the user what's going on as early as possible).
	 */
	errorcode_t err;
	(void)err; /* Prevent warning if no input/output driver is set */
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
	err = device_attach_single(input_dev);
	if (err != ANANAS_ERROR_OK) {
		device_free(input_dev);
		input_dev = NULL;
	}
#endif /* CONSOLE_INPUT_DRIVER */

#ifdef CONSOLE_OUTPUT_DRIVER
#ifdef CONSOLE_INPUT_DRIVER
	if (strcmp(STRINGIFY(CONSOLE_INPUT_DRIVER), STRINGIFY(CONSOLE_OUTPUT_DRIVER)) == 0) {
		output_dev = input_dev;
	} else {
#endif
		extern struct DRIVER CONSOLE_OUTPUT_DRIVER;
		output_dev = device_alloc(NULL, &CONSOLE_OUTPUT_DRIVER);
#ifdef CONSOLE_OUTPUT_BUS
		sprintf(tmphint, "%s.%s.%u.", CONSOLE_OUTPUT_BUS, output_dev->name, output_dev->unit);
#else
		sprintf(tmphint, "%s.%u.", output_dev->name, output_dev->unit);
#endif
		device_get_resources_byhint(output_dev, tmphint, config_hints);
		err = device_attach_single(output_dev);
		if (err != ANANAS_ERROR_OK) {
			device_free(output_dev);
			output_dev = NULL;
		}
#ifdef CONSOLE_INPUT_DRIVER
	}
#endif
#endif /* CONSOLE_DRIVER */
	if (input_dev != NULL || output_dev != NULL)
		console_tty = tty_alloc(input_dev, output_dev);
}

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

/* vim:set ts=2 sw=2: */
