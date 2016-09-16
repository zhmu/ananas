#include <stdlib.h>

double
atof(const char* str)
{
	return strtod(str, (char**)NULL);
}
