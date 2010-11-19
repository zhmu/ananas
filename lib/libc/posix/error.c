#include <ananas/types.h>
#include <ananas/error.h>
#include <_posix/error.h>
#include <errno.h>

void
_posix_map_error(errorcode_t err)
{
#define SET_ERRNO(x) do { errno = (x); return; } while(0)
	switch(ANANAS_ERROR_CODE(err)) {
		case ANANAS_ERROR_NONE:
			return;
		case ANANAS_ERROR_BAD_HANDLE:
			SET_ERRNO(EBADF);
		case ANANAS_ERROR_BAD_ADDRESS:
			SET_ERRNO(EFAULT);
		case ANANAS_ERROR_OUT_OF_HANDLES:
			SET_ERRNO(EMFILE);
		case ANANAS_ERROR_NO_FILE:
			SET_ERRNO(ENOENT);
		case ANANAS_ERROR_NOT_A_DIRECTORY:
			SET_ERRNO(ENOTDIR);
		case ANANAS_ERROR_BAD_EXEC:
			SET_ERRNO(ENOEXEC);
		case ANANAS_ERROR_BAD_TYPE:
			SET_ERRNO(EINVAL);
		case ANANAS_ERROR_BAD_OPERATION:
			SET_ERRNO(EINVAL);
		case ANANAS_ERROR_BAD_RANGE:
			SET_ERRNO(ERANGE);
		case ANANAS_ERROR_SHORT_READ:
			SET_ERRNO(EOVERFLOW);
		case ANANAS_ERROR_BAD_SYSCALL:
			SET_ERRNO(ENOSYS);
		case ANANAS_ERROR_BAD_LENGTH:
		case ANANAS_ERROR_BAD_FLAG:
			SET_ERRNO(EINVAL);
		case ANANAS_ERROR_IO:
			SET_ERRNO(EIO);
		case ANANAS_ERROR_NO_DEVICE:
		case ANANAS_ERROR_NO_RESOURCE:
			SET_ERRNO(ENXIO);
		case ANANAS_ERROR_CLONED: /* should never end up here */
		case ANANAS_ERROR_UNKNOWN:
		default:
			SET_ERRNO(EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
