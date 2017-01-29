#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <ananas/device.h>
#include <ananas/list.h>

void console_putchar(int c);
void console_putstring(const char* s);
uint8_t console_getchar();

extern device_t console_tty;

/*
 * Console drivers are 'special' as they will be probed during early boot and
 * skipped afterwards.
 *
 * The priority influences the order in which these things are attached, the
 * first successful driver wins and becomes the console driver. The order is
 * highest first, so use zero for the 'if all else fails'-driver.
 */
struct CONSOLE_DRIVER {
	struct DRIVER*	con_driver;
	int		con_priority;
	int		con_flags;
#define CONSOLE_FLAG_IN		0x0001
#define CONSOLE_FLAG_OUT	0x0002
#define CONSOLE_FLAG_INOUT	(CONSOLE_FLAG_IN | CONSOLE_FLAG_OUT)
	LIST_FIELDS(struct CONSOLE_DRIVER);
};
LIST_DEFINE(CONSOLE_DRIVERS, struct CONSOLE_DRIVER);

#define DEFINE_CONSOLE_DRIVER(drv, prio, flags) \
	static struct CONSOLE_DRIVER condrv_##drv; \
	static errorcode_t register_condrv_##drv() { \
		return console_register_driver(&condrv_##drv); \
	} \
	static errorcode_t unregister_condrv_##drv() { \
		return console_unregister_driver(&condrv_##drv); \
	} \
	INIT_FUNCTION(register_condrv_##drv, SUBSYSTEM_CONSOLE, ORDER_FIRST); \
	EXIT_FUNCTION(unregister_condrv_##drv); \
	static struct CONSOLE_DRIVER condrv_##drv = { \
		.con_driver = &drv, \
		.con_priority = prio, \
		.con_flags = flags \
	};

errorcode_t console_register_driver(struct CONSOLE_DRIVER* con);
errorcode_t console_unregister_driver(struct CONSOLE_DRIVER* con);

#endif /* __CONSOLE_H__ */
