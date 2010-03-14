#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

static const uint8_t hextab_hi[16] = "0123456789ABCDEF";
static const uint8_t hextab_lo[16] = "0123456789abcdef";

/* from dtoa.c */
char* dtoa(double dd, int mode, int ndigits, int *decpt, int *sign, char **rve);

int
puts(const char* s)
{
	while(*s)
		putchar(*s++);
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
putint(void(*putch)(void*, int), void* v, unsigned int n)
{
	/*
	 * Note that 1234 is just 1*10^3 + 2*10^2 + 3*10^1 + 4*10^0 =
	 * 1000 + 200 + 30 + 4. This means we have to figure out the highest power p
	 * of 10 first (p=3 in this case) and then print 'n divide. The digit we
	 * need to print is n % 10^p, so 1234 % 10^3 = 1, 234 % 10^2 = 2 etc)
	 */
	unsigned int i, p = 0, base = 1;
	for (i = n; i > 10; i /= 10, p++, base *= 10);
	/* Write values from n/(p^10) .. n/1 */
	for (i = 0; i<= p; i++, base /= 10) {
		putch(v, '0' + (n / base) % 10);
	}
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
		/* XXX skip '.', '*' precision stuff */
		while ((*fmt >= '0' && *fmt <= '9') || *fmt == '*' || *fmt == '.') fmt++;
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
			case 'f': /* float */
			case 'g': { /* float */
					int sign, decpt;
					char* tmp = dtoa(va_arg(ap, double), (*fmt == 'f') ? 3 : 2, 6, &decpt, &sign, NULL);
					if (sign) putch(v, '-');
					while (*tmp) {
						if (decpt-- == 0)
							putch(v, '.');
						putch(v, *tmp++);
					}
				}
				break;
			case 'x': /* hex int XXX assumed 32 bit */
				putnumber(putch, v, hextab_lo, va_arg(ap, unsigned int));
				break;
			case 'X': /* hex int XXX assumed 32 bit */
				putnumber(putch, v, hextab_hi, va_arg(ap, unsigned int));
				break;
			case 'u': /* unsigned integer */
			case 'i': /* integer XXX */
				putint(putch, v, va_arg(ap, unsigned int));
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
va_putchar(void* v, int c) {
	putchar(c);
}

void
vaprintf(const char* fmt, va_list ap)
{
	vapprintf(fmt, va_putchar, NULL, ap);
}

int
printf(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);

	return 0; /* XXX */
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
