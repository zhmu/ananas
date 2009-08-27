#ifndef __CONSOLE_H__
#define __CONSOLE_H__

/* Base memory address of the console memory */
#define CONSOLE_MEM_BASE 0xb8000

/* Video memory length */
#define CONSOLE_MEM_LENGTH 4000

/* Console height */
#define CONSOLE_HEIGHT	25

/* Console width */
#define CONSOLE_WIDTH	80

void console_init();
void console_putc(char c);

#endif /* __CONSOLE_H__ */
