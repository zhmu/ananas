#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <ananas/stdint.h>

namespace Ananas {
class Device;
}

void console_putchar(int c);
void console_putstring(const char* s);
uint8_t console_getchar();

extern Ananas::Device* console_tty;


#endif /* __CONSOLE_H__ */
