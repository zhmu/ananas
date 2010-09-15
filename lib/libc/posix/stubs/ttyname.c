#include <unistd.h>

char*
ttyname(int fildes)
{
	return "console";
}
