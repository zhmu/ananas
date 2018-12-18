#ifndef ANANAS_ERRNO_H
#define ANANAS_ERRNO_H

// POSIX error values
#define E2BIG 1
#define EACCES 2
#define EADDRINUSE 3
#define EADDRNOTAVAIL 4
#define EAFNOSUPPORT 5
#define EAGAIN 6
#define EALREADY 7
#define EBADF 8
#define EBADMSG 9
#define EBUSY 10
#define ECANCELED 11
#define ECHILD 12
#define ECONNABORTED 13
#define ECONNREFUSED 14
#define ECONNRESET 15
#define EDEADLK 16
#define EDESTADDRREQ 17
#define EDOM 18
#define EDQUOT 19
#define EEXIST 20
#define EFAULT 21
#define EFBIG 22
#define EHOSTUNREACH 23
#define EIDRM 24
#define EILSEQ 25
#define EINPROGRESS 26
#define EINTR 27
#define EINVAL 28
#define EIO 29
#define EISCONN 30
#define EISDIR 31
#define ELOOP 32
#define EMFILE 33
#define EMLINK 34
#define EMSGSIZE 35
#define EMULTIHOP 36
#define ENAMETOOLONG 37
#define ENETDOWN 38
#define ENETRESET 39
#define ENETUNREACH 40
#define ENFILE 41
#define ENOBUFS 42
#define ENODATA 43
#define ENODEV 44
#define ENOENT 45
#define ENOEXEC 46
#define ENOLCK 47
#define ENOLINK 48
#define ENOMEM 49
#define ENOMSG 50
#define ENOPROTOOPT 51
#define ENOSPC 52
#define ENOSR 53
#define ENOSTR 54
#define ENOSYS 55
#define ENOTCONN 56
#define ENOTDIR 57
#define ENOTEMPTY 58
#define ENOTSOCK 59
#define ENOTSUP 60
#define ENOTTY 61
#define ENXIO 62
#define EOPNOTSUPP 63
#define EOVERFLOW 64
#define EPERM 65
#define EPIPE 66
#define EPROTO 67
#define EPROTONOSUPPORT 68
#define EPROTOTYPE 69
#define ERANGE 70
#define EROFS 71
#define ESPIPE 72
#define ESRCH 73
#define ESTALE 74
#define ETIME 75
#define ETIMEDOUT 76
#define ETXTBSY 77
#define EWOULDBLOCK EAGAIN
#define EXDEV 78

/* Not in POSIX, but useful nevertheless: last errno we have defined */
#define ELAST 78

#endif /* ANANAS_ERRNO_H */
