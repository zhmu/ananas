#include <ananas/device.h>

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

void console_putchar(int c);
uint8_t console_getchar();

extern device_t console_tty;

#endif /* __CONSOLE_H__ */
