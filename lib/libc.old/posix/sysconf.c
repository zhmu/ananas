#include <unistd.h>
#include <machine/param.h>

long sysconf(int name)
{
	switch(name) {
		case _SC_PAGESIZE:
			return PAGE_SIZE;
		default:
			return -1;
	}
}

/* vim:set ts=2 sw=2: */
