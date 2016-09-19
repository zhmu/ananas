#include <ananas/process.h>

int getdtablesize()
{
	return PROCESS_MAX_HANDLES;
}
