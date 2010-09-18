#include <ananas/lib.h>
#include <ananas/schedule.h>

void
panic(const char* fmt, ...)
{
	va_list ap;
	char hack[128 /* XXXX */];

	/* disable the scheduler - this ensures any extra BSP's will not run threads either */
	scheduler_deactivate();

	strcpy(hack, "panic: ");
	strcat(hack, fmt);
	strcat(hack, "\n");
	va_start(ap, fmt);
	vaprintf(hack, ap);
	va_end(ap);

	while(1);
}
