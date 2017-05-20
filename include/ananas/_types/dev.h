#ifndef __DEV_T_DEFINED
typedef __uint32_t	dev_t;
#define __DEV_T_DEFINED

#define major(x)	((x) >> 16)
#define minor(x)	((x) & 0xffff)
#define makedev(ma,mi)	(((ma) << 16) | ((mi) & 0xffff))

#endif
