#include "io.h"
#include "types.h"

#define VIDEO_BASE 0xb8000
#define VIDEO_ROWS 25
#define VIDEO_COLS 80

static const char hextab[] = "0123456789abcdef";
static int x = 0;
static int y = 0;

typedef __builtin_va_list va_list;
#define va_arg __builtin_va_arg
#define va_start __builtin_va_start
#define va_end __builtin_va_end

int putchar(int ch)
{
    switch (ch) {
        case '\n':
            x = 0;
            y++;
            break;
        default:
            *(uint16_t*)(VIDEO_BASE + (y * VIDEO_COLS * 2) + x * 2) = 0x0700 | (ch & 0xff);
            x++;
            break;
    }

    if (x >= VIDEO_COLS) {
        x = 0;
        y++;
    }

    if (y >= VIDEO_ROWS) {
        y = 0;
        /* XXX implement scrolling */
    }
    return 0;
}

int puts(const char* s)
{
    while (*s) {
        putchar(*s++);
    }
    return 0;
}

static void put8(uint8_t v)
{
    putchar(hextab[v >> 4]);
    putchar(hextab[v & 0xf]);
}

void putx(uint32_t v)
{
    put8(v >> 24);
    put8((v >> 16) & 0xff);
    put8((v >> 8) & 0xff);
    put8(v & 0xff);
}

static int vprintf(const char* fmt, va_list va)
{
    while (*fmt != '\0') {
        if (*fmt != '%') {
            putchar(*fmt++);
            continue;
        }

        fmt++;
        switch (*fmt) {
            case 'p': /* pointers are always 32-bit here */
            case 'x': {
                uint32_t v = va_arg(va, uint32_t);
                if (v >= 0x10000000)
                    put8(v >> 24);
                if (v >= 0x10000)
                    put8((v >> 16) & 0xff);
                if (v >= 0x100)
                    put8((v >> 8) & 0xff);
                put8(v & 0xff);
                break;
            }
            case 's': {
                const char* s = va_arg(va, const char*);
                while (*s != '\0')
                    putchar(*s++);
                break;
            }
            case 'd': {
                int d = va_arg(va, int);
                unsigned int divisor = 1, p = 0;
                for (unsigned int n = d; n >= 10; n /= 10, divisor *= 10, p++)
                    /* nothing */;
                for (unsigned int i = 0; i <= p; i++, divisor /= 10)
                    putchar(hextab[(d / divisor) % 10]);
                break;
            }
            default:
                putchar('%');
                putchar(*fmt);
                break;
        }
        fmt++;
    }

    return 0;
}

int printf(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);
    return 0;
}

void panic(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);

    printf("fatal: ");
    vprintf(fmt, va);
    va_end(va);

    while (1) /* nothing */
        ;
}
