#ifndef __ANANAS_ERROR_H__
#define __ANANAS_ERROR_H__

#include <ananas/trace.h>

#define ANANAS_ERROR_NONE		0		/* All is fine */
#define ANANAS_ERROR_BAD_HANDLE		1		/* Supplied handle is invalid or bad type */
#define ANANAS_ERROR_BAD_ADDRESS	2		/* Supplied pointer is invalid */
#define ANANAS_ERROR_OUT_OF_HANDLES	3		/* Cannot allocate handle */
#define ANANAS_ERROR_NO_FILE		4		/* File not found */
#define ANANAS_ERROR_NOT_A_DIRECTORY	5		/* Not a directory */
#define ANANAS_ERROR_BAD_EXEC		6		/* Couldn't execute handle */
#define ANANAS_ERROR_BAD_TYPE		7		/* Unknown type */
#define ANANAS_ERROR_BAD_OPERATION	8		/* Operation cannot be performed in this context */
#define ANANAS_ERROR_BAD_RANGE		9		/* Argument range invalid */
#define ANANAS_ERROR_SHORT_READ		10		/* More data expected */
#define ANANAS_ERROR_BAD_SYSCALL	11		/* Unsupported/unimplemented system call */
#define ANANAS_ERROR_BAD_LENGTH		12		/* Argument length invalid */
#define ANANAS_ERROR_UNKNOWN		13		/* Shouldn't happen :-) */
#define ANANAS_ERROR_BAD_FLAG		14		/* Unsupported flags given */
#define ANANAS_ERROR_CLONED		15		/* Result code for a cloned thread call */
#define ANANAS_ERROR_IO			16		/* General I/O error */
#define ANANAS_ERROR_NO_DEVICE		17		/* Device not found */
#define ANANAS_ERROR_NO_RESOURCE	18		/* Resources cannot be found or acquired */
#define ANANAS_ERROR_FILE_EXISTS	19		/* File already exists */
#define ANANAS_ERROR_NO_SPACE		20		/* Out of space */
#define ANANAS_ERROR_OUT_OF_MEMORY	21		/* Out of memory */
#define ANANAS_ERROR_CROSS_DEVICE	22		/* Cross device operation */
#define ANANAS_ERROR_UNSUPPORTED	23		/* Unsupported operation */

static inline errorcode_t ananas_success()
{
	return ANANAS_ERROR_NONE;
}

static inline int ananas_is_success(errorcode_t err)
{
	return err == ANANAS_ERROR_NONE;
}

static inline int ananas_is_failure(errorcode_t err)
{
	return !ananas_is_success(err);
}

static inline errorcode_t ananas_make_error(int type)
{
	return type;
}

#define ANANAS_ERROR_RETURN(x) \
	if(ananas_is_failure((x))) \
		return (x)

/* OK is special; it does not have any line information */
#define ANANAS_ERROR(x) \
	(((TRACE_FILE_ID * 1000000) + (__LINE__) * 100) + ((ANANAS_ERROR_##x)))

#define ANANAS_ERROR_CODE(x) \
	((x) % 100)
#define ANANAS_ERROR_LINE(x) \
	(((x) / 100) % 1000)
#define ANANAS_ERROR_FILEID(x) \
	((x) / 1000000)

#endif /* __ANANAS_ERROR_H__ */
