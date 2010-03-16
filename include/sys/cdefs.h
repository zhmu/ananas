#ifndef __SYS_CDEFS_H__
#define __SYS_CDEFS_H__

#define __dead		__attribute__((__noreturn__))
#define __unused	__attribute__((__unused__))
#define __packed	__attribute__((__packed__))
#define __aligned(x)	__attribute__((__aligned__(x)))

#endif /* __SYS_CDEFS_H__ */
