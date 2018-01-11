#include <unistd.h>
#include <machine/param.h>

long sysconf(int name)
{
	switch(name) {
		case _SC_PAGESIZE:
			return PAGE_SIZE;
		case _SC_CLK_TCK:
			return 1; /* TODO */
		default:
			return -1;
	}
}

/* vim:set ts=2 sw=2: */
