#include <ananas/thread.h>

int getdtablesize()
{
	return THREAD_MAX_HANDLES;
}
