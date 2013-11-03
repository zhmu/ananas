#include <ananas/types.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <stdarg.h>

static const char hextab_hi[16] = "0123456789ABCDEF";
static const char hextab_lo[16] = "0123456789abcdef";

int
puts(const char* s)
{
	while(*s)
		console_putchar(*s++);
	return 0; /* can't fail */
}

static void
putint(void(*putch)(void*, int), void* v, int base, const char* tab, int min_field_len, char pad, uintmax_t n)
{
	/*
	 * Note that 1234 is just 1*10^3 + 2*10^2 + 3*10^1 + 4*10^0 =
	 * 1000 + 200 + 30 + 4. This means we have to figure out the highest power p
	 * of 10 first (p=3 in this case) and then print 'n divide. The digit we
	 * need to print is n % 10^p, so 1234 % 10^3 = 1, 234 % 10^2 = 2 etc)
	 */
	uintmax_t divisor = 1;
	unsigned int p = 0, digits = 1;
	for (uintmax_t i = n; i >= base; i /= base, p++, divisor *= base, digits++);
	/* Now that we know the length, deal with the padding */
	for (unsigned int i = digits; i < min_field_len; i++)
		putch(v, pad);
	/* Write values from n/(p^base) .. n/1 */
	for (unsigned int i = 0; i <= p; i++, divisor /= base) {
		putch(v, tab[(n / divisor) % base]);
	}
}

static void
putstr(void(*putch)(void*, int), void* v, int min_field_len, unsigned int precision, char pad, const char* s)
{
	for (unsigned int i = strlen(s); i < min_field_len; i++)
		putch(v, pad);
	while (*s && precision-- > 0)
		putch(v, *s++);
}

static void
vapprintf(const char* fmt, void(*putch)(void*, int), void* v, va_list ap)
{
	const char* s;
	register uintmax_t n;

	while(*fmt) {
		if (*fmt != '%') {
			putch(v, *fmt++);
			continue;
		}

		/* formatted output */
		fmt++;

		unsigned int min_field_width = 0;
		unsigned int precision = 0;
		char padding = ' ';

		/* Try to handle flags */
		switch(*fmt) {
			case '#': /* Alternate form */
				fmt++;
				break;
			case '0': /* Zero padding */
				padding = '0';
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
			case '\'': /* Decimal */
				fmt++;
				break;
		}

#define DIGIT(x) ((x) >= '0' && (x) <= '9')
		/* Handle minimum width */
		while (DIGIT(*fmt)) {
			min_field_width *= 10;
			min_field_width += (*fmt - '0');
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
				if (s == NULL)
					s = "(null)";
				if (!precision) precision = (unsigned int)-1;
				putstr(putch, v, min_field_width, precision, padding, s);
				break;
			case 'c': /* char (upcast to unsigned int) */
				n = va_arg(ap, unsigned int);
				putch(v, n);
				break;
			case 'x': /* hex int */
				n = va_arg(ap, unsigned int);
				putint(putch, v, 16, hextab_lo, min_field_width, padding, n);
				break;
			case 'X': /* hex int XXX assumed 32 bit */
				n = va_arg(ap, unsigned int);
				putint(putch, v, 16, hextab_hi, min_field_width, padding, n);
				break;
			case 'u': /* unsigned integer */
				n = va_arg(ap, unsigned int);
				putint(putch, v, 10, hextab_lo, min_field_width, padding, n);
				break;
			case 'o': /* octal */
				n = va_arg(ap, unsigned int);
				putint(putch, v, 8, hextab_lo, min_field_width, padding, n);
				break;
			case 'd': /* signal decimal */
			case 'i': /* signal integer */
			{
				int i = va_arg(ap, int);
				if (i < 0) {
					/* Negative: flip the sign, show the line */
					i = -i;
					putch(v, '-');
				}
				putint(putch, v, 10, hextab_lo, min_field_width, padding, i);
				break;
			}
			case 'p': /* pointer */
				n = (uintmax_t)(addr_t)va_arg(ap, void*);
				putint(putch, v, 16, hextab_lo, min_field_width, padding, n);
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

struct SNPRINTF_CTX {
	char* buf;
	int cur_len;
	size_t left;
};

static void
snprintf_add(void* v, int c)
{
	struct SNPRINTF_CTX* ctx = v;
	if (ctx->left == 0)
		return;

	*ctx->buf = c;
	ctx->buf++;
	ctx->cur_len++;
	ctx->left--;
}

int
snprintf(char* str, size_t len, const char* fmt, ...)
{
	struct SNPRINTF_CTX ctx;

	ctx.buf = str;
	ctx.cur_len = 0;
	ctx.left = len;
	
	va_list ap;
	va_start(ap, fmt);
	vapprintf(fmt, snprintf_add, &ctx, ap);
	va_end(ap);
	snprintf_add(&ctx, 0);

	return ctx.cur_len - 1 /* minus \0 byte */;
}

/* vim:set ts=2 sw=2: */
