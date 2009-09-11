#include "types.h"
#include "console.h"
#include "device.h"
#include "stdarg.h"

static const uint8_t hextab_hi[16] = "0123456789ABCDEF";
static const uint8_t hextab_lo[16] = "0123456789abcdef";

int
puts(const char* s)
{
	while(*s)
		console_putchar(*s++);
	return 0; /* can't fail */
}

static void
putnumber(void(*putch)(void*, int), void* v, const char* tab, unsigned int i)
{
	if (i >= 0x10000000) putch(v, tab[(i >> 28) & 0xf]);
	if (i >= 0x1000000)  putch(v, tab[(i >> 24) & 0xf]);
	if (i >= 0x100000)   putch(v, tab[(i >> 20) & 0xf]);
	if (i >= 0x10000)    putch(v, tab[(i >> 16) & 0xf]);
	if (i >= 0x1000)     putch(v, tab[(i >> 12) & 0xf]);
	if (i >= 0x100)      putch(v, tab[(i >>  8) & 0xf]);
	if (i >= 0x10)       putch(v, tab[(i >>  4) & 0xf]);
	putch(v, tab[i & 0xf]);
}

static void
vapprintf(const char* fmt, void(*putch)(void*, int), void* v, va_list ap)
{
	const char* s;
	unsigned int i;

	while(*fmt) {
		if (*fmt != '%') {
			putch(v, *fmt++);
			continue;
		}

		/* formatted output */
		fmt++;
		switch(*fmt) {
			case 's': /* string */
				s = va_arg(ap, const char*);
				if (s != NULL) {
					while (*s)
						putch(v, *s++);
				} else {
						putch(v, '(');
						putch(v, 'n'); putch(v, 'u'); putch(v, 'l'); putch(v, 'l');
						putch(v, ')');
				}
				break;
			case 'c': /* char */
				putch(v, va_arg(ap, unsigned int));
				break;
			case 'x': /* hex int XXX assumed 32 bit */
				putnumber(putch, v, hextab_lo, va_arg(ap, unsigned int));
				break;
			case 'X': /* hex int XXX assumed 32 bit */
				putnumber(putch, v, hextab_hi, va_arg(ap, unsigned int));
				break;
			default: /* unknown, just print it */
				putch(v, '%');
				putch(v, *fmt);
				break;
		}
		fmt++;
	}
}

static void
kprintf_putch(void* v, int c)
{
	console_putchar(c);
}

void
vaprintf(const char* fmt, va_list ap)
{
	vapprintf(fmt, kprintf_putch, NULL, ap);
}

void
kprintf(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);
}

static void
sprintf_add(void* v, int c)
{
	char** string = (char**)v;
	**string = c;
	(*string)++;
}

int
sprintf(char* str, const char* fmt, ...)
{
	va_list ap;
	char* source = str;

	va_start(ap, fmt);
	vapprintf(fmt, sprintf_add, &str, ap);
	va_end(ap);
	sprintf_add(&str, 0);

	return str - source;
}

/* vim:set ts=2 sw=2: */
