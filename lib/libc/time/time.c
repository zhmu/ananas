#include <time.h>

time_t
time(time_t* loc)
{
	/* TODO */
	time_t curtime = (time_t)-1;
	if (loc != NULL)
		*loc = curtime;
	return curtime;
}
