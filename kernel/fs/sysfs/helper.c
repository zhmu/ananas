#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <machine/param.h>

errorcode_t
sysfs_read_string(char* string, char** start, off_t offset, size_t* len)
{
	int slen = strlen(string);
	KASSERT(slen < PAGE_SIZE, "string exceeds one page");
	if (offset >= slen) {
		/* No space left */
		*len = 0;
		return ANANAS_ERROR_NONE;
	}
	*len = strlen(string) - slen;
	*start = (char*)string + offset;
	return ANANAS_ERROR_NONE;
}

/* vim:set ts=2 sw=2: */
