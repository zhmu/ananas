#include <string.h>
#include <stdlib.h>

char* strdup(const char* s1)
{
	int len = strlen(s1);
	char* ptr = malloc(len + 1);
	if (ptr == NULL)
		return NULL;
	memcpy(ptr, s1, len + 1 /* include terminating \0 */);
	return ptr;
}
