#include "dev/console.h"

void
mi_startup()
{
	console_init();

	while (1);
}
