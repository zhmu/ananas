#include "lib.h"

void
panic(const char* fmt, ...)
{
	va_list ap;

	kprintf("panic: ");
	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);

	while(1);
}
