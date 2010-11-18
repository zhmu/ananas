#include <ananas/types.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/lock.h>
#include <stdarg.h>

static const uint8_t hextab_hi[16] = "0123456789ABCDEF";
static const uint8_t hextab_lo[16] = "0123456789abcdef";

struct SPINLOCK spl_print = { 0 };

int
puts(const char* s)
{
	while(*s)
		console_putchar(*s++);
	return 0; /* can't fail */
}

static void
putnumber(void(*putch)(void*, int), void* v, const uint8_t* tab, unsigned int i)
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
	for (i = n; i >= 10; i /= 10, p++, base *= 10);
	/* Write values from n/(p^10) .. n/1 */
	for (i = 0; i<= p; i++, base /= 10) {
		putch(v, '0' + (n / base) % 10);
	}
}

static void
vapprintf(const char* fmt, void(*putch)(void*, int), void* v, va_list ap)
{
	const char* s;
	uint64_t u64;

	while(*fmt) {
		if (*fmt != '%') {
			putch(v, *fmt++);
			continue;
		}

		/* formatted output */
		fmt++;

		unsigned int min_width = 0;
		unsigned int precision = 0;

		/* Try to handle flags */
		switch(*fmt) {
			case '#': /* Alternate form */
				fmt++;
				break;
			case '0': /* Zero padding */
				fmt++;
				break;
			case '-': /* Negative field length */
				fmt++;
				break;
			case ' ': /* Blank left */
				fmt++;
				break;
			case '+': /* Sign */
				fmt++;
				break;
			case '\'': /* Decimal*/
				fmt++;
				break;
		}

#define DIGIT(x) ((x) >= '0' && (x) <= '9')
		/* Handle minimum width */
		while (DIGIT(*fmt)) {
			min_width *= 10;
			min_width += (*fmt - '0');
			fmt++;
		}

		/* Precision */
		if (*fmt == '.') {
			fmt++;
			while (DIGIT(*fmt)) {
				precision *= 10;
				precision += (*fmt - '0');
				fmt++;
			}

		}

		switch(*fmt) {
			case 's': /* string */
				s = va_arg(ap, const char*);
				if (s != NULL) {
					if (!precision) precision = (unsigned int)-1;
					while (*s && precision-- > 0)
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
			case 'u': /* unsigned integer */
			case 'd': /* decimal */
			case 'i': /* integer XXX */
				putint(putch, v, va_arg(ap, unsigned int));
				break;
			case 'p': /* pointer XXX assumed 64 bit */
				u64 = va_arg(ap, addr_t);
#ifdef __amd64__
				/* XXXPORTABILITY */
				if (u64 >= (1L << 32))
					putnumber(putch, v, hextab_lo, u64 >> 32);
#endif
				putnumber(putch, v, hextab_lo, u64 & 0xffffffff);
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
	spinlock_lock(&spl_print);
	vapprintf(fmt, kprintf_putch, NULL, ap);
	spinlock_unlock(&spl_print);
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
