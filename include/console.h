#include "device.h"

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

void console_init();
void console_putchar(int c);

extern device_t console_dev;

#endif /* __CONSOLE_H__ */
