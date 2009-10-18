#include "lib.h"

void
panic(const char* fmt, ...)
{
	va_list ap;
	char hack[128 /* XXXX */];

	strcpy(hack, "panic: ");
	strcat(hack, fmt);
	strcat(hack, "\n");
	va_start(ap, fmt);
	vaprintf(hack, ap);
	va_end(ap);

	while(1);
}
