#ifndef __ERRNO_H__
#define __ERRNO_H__

/* XXX: this must be restructed for multiple thread support! */
extern int errno;

/*
 * All error codes as described by POSIX. Note that most of them cannot occur
 * yet.
 */
#define E2BIG		1
#define EACCES		2
#define EADDRINUSE	3
#define EADDRNOTAVAIL	4
#define EAFNOSUPPORT	5
#define EAGAIN		6
#define EALREADY	7
#define EBADF		8
#define EBADMSG		9
#define EBUSY		10
#define ECANCELLED	11
#define ECHILD		12
#define ECONNABORTED	13
#define ECONNREFUSED	14
#define ECONNRESET	15
#define EDEADLK		16
#define EDESTADDRREQ	17
#define EDOM		18
#define EDQUOT		19
#define EEXIST		20
#define EFAULT		21
#define EFBIG		22
#define EHOSTUNREACH	23
#define EIDRM		24
#define EILSEQ		25
#define EINPROGRESS	26
#define EINTR		27
#define EINVAL		28
#define EIO		29
#define EISCONN		30
#define EISDIR		31
#define ELOOP		32
#define EMFILE		33
#define EMLINK		34
#define EMSGSIZE	35
#define EMULTIHOP	36
#define ENAMETOOLONG	37
#define ENETDOWN	38
#define ENETRESET	39
#define ENETUNREACH	40
#define ENFILE		41
#define ENOBUFS		42
#define ENODATA		43
#define ENODEV		44
#define ENOENT		45
#define ENOEXEC		46
#define ENOLCK		47
#define ENOLINK		48
#define ENOMEM		49
#define ENOPROTOOPT	50
#define ENOSPC		51
#define ENOSYS		52
#define ENOTCONN	53
#define ENOTDIR		54
#define ENOTEMPTY	55
#define ENOTTY		56
#define ENXIO		57
#define EOPNOTSUPP	58
#define EOVERFLOW	59
#define EPERM		60
#define EPIPE		61
#define EPROTO		62
#define EPROTONOSUPPORT	63
#define EPROTOTYPE	64
#define ERANGE		65
#define EROFS		66
#define ESPIPE		67
#define ESRCH		68
#define ESTALE		69
#define ETIMEDOUT	70
#define ETXTBSY		71
#define EWOULDBLOCK	72
#define EXDEV		73

#endif /* __ERRNO_H__ */
