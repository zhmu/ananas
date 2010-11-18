#include <ananas/lib.h>
#include <ananas/schedule.h>

void
_panic(const char* file, const char* func, int line, const char* fmt, ...)
{
#if defined(__i386__) || defined(__amd64__)
	/* XXX This is a horrible hack */
	__asm ("cli");
#endif
	va_list ap;
	/* disable the scheduler - this ensures any extra BSP's will not run threads either */
	scheduler_deactivate();

	kprintf("panic in %s:%u (%s): ", file, line, func);

	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);

	while(1);
}
