#include "types.h"
#include "stdarg.h"
#include "device.h"
#include "dev/console.h"

int
putchar(int c)
{
	extern ssize_t sio_write(device_t dev, const char* data, size_t len);
	sio_write(NULL, (unsigned char*)&c, 1);
	return c;

}

static const uint8_t hextab[16] = "0123456789ABCDEF";

int
puts(const char* s)
{
	while(*s)
		putchar(*s++);
	return 0; /* can't fail */
}

void
vaprintf(const char* fmt, va_list ap)
{
	const char* s;
	unsigned int i;

	while(*fmt) {
		if (*fmt != '%') {
			putchar(*fmt++);
			continue;
		}

		/* formatted output */
		fmt++;
		switch(*fmt) {
			case 's': /* string */
				s = va_arg(ap, const char*);
				puts(s == NULL ? "(null)" : s);
				break;
			case 'c': /* char */
				putchar(va_arg(ap, unsigned int));
				break;
			case 'x': /* hex int XXX assumed 32 bit */
				i = va_arg(ap, unsigned int);
				if (i >= 0x10000000) putchar(hextab[(i >> 28) & 0xf]);
				if (i >= 0x1000000)  putchar(hextab[(i >> 24) & 0xf]);
				if (i >= 0x100000)   putchar(hextab[(i >> 20) & 0xf]);
				if (i >= 0x10000)    putchar(hextab[(i >> 16) & 0xf]);
				if (i >= 0x1000)     putchar(hextab[(i >> 12) & 0xf]);
				if (i >= 0x100)      putchar(hextab[(i >>  8) & 0xf]);
				if (i >= 0x10)       putchar(hextab[(i >>  4) & 0xf]);
				putchar(hextab[i & 0xf]);
				break;
			default: /* unknown, just print it */
				putchar('%');
				putchar(*fmt);
				break;
		}
		fmt++;
	}
}

void
kprintf(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);
}

/* vim:set ts=2 sw=2: */
