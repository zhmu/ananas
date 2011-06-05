#ifndef __SYS_HANDLE_OPTIONS_H__
#define __SYS_HANDLE_OPTIONS_H__

struct CLONE_OPTIONS {
	size_t		cl_size;
};

struct SUMMON_OPTIONS {
	size_t		su_size;		/* structure length, in bytes */
	unsigned int	su_flags;		/* summon flags */
#define SUMMON_FLAG_RUNNING	0x0001		/* do not suspend thread initially */
	size_t		su_args_len;		/* argument length, or 0 */
	const char*	su_args;		/* arguments, or NULL */
	size_t		su_env_len;		/* environment length, or 0 */
	const char*	su_env;			/* environment, or NULL */
};

struct CREATE_OPTIONS {
	size_t		cr_size;		/* structure length, in bytes */
	unsigned int	cr_type;		/* what to create */
#define CREATE_TYPE_FILE	0x0001
#define CREATE_TYPE_MEMORY	0x0002
	const char*	cr_path;		/* path to creation, if any */
	unsigned int	cr_flags;		/* flags, if any */
#define CREATE_MEMORY_FLAG_READ		0x0001
#define CREATE_MEMORY_FLAG_WRITE	0x0002
#define CREATE_MEMORY_FLAG_EXECUTE	0x0004
#define CREATE_MEMORY_FLAG_MASK \
	(CREATE_MEMORY_FLAG_READ | CREATE_MEMORY_FLAG_WRITE | CREATE_MEMORY_FLAG_EXECUTE)
	size_t		cr_length;		/* length of creation, if any */
	unsigned int	cr_mode;		/* mode, if any */
};

struct OPEN_OPTIONS {
	size_t		op_size;		/* structure length, in bytes */
	const char*	op_path;		/* path to open */
	unsigned int	op_mode;		/* open mode */
#define OPEN_MODE_NONE		0x0000
#define OPEN_MODE_READ		0x0001		/* Read only */
#define OPEN_MODE_WRITE		0x0002		/* Write only */
#define OPEN_MODE_DIRECTORY	0x0004		/* Open as a directory */
#define OPEN_MODE_CREATE	0x0008		/* Create if file not exists */
#define OPEN_MODE_EXCLUSIVE	0x0010		/* Call must create the file */
#define OPEN_MODE_TRUNCATE	0x0020		/* Truncate existing file */
	unsigned int	op_createmode;
};

/* Handle options - generic */
#define _HCTL_GENERIC_FIRST	1
#define HCTL_GENERIC_SETOWNER	(_HCTL_GENERIC_FIRST+0)	/* Change handle ownership */
#define _HCTL_GENERIC_LAST	99

/* Handle options - files */
#define _HCTL_FILE_FIRST	100
#define HCTL_FILE_SETCWD	(_HCTL_FILE_FIRST+0)	/* Activate handle as current working directory */
#define HCTL_FILE_SEEK		(_HCTL_FILE_FIRST+1)	/* Seek to a position in the handle */
struct HCTL_SEEK_ARG {
	int		se_whence;
#define HCTL_SEEK_WHENCE_SET	0
#define HCTL_SEEK_WHENCE_CUR	1
#define HCTL_SEEK_WHENCE_END	2
	off_t*		se_offs;
};
#define HCTL_FILE_STAT		(_HCTL_FILE_FIRST+2)	/* Seek to a position in the handle */
struct HCTL_STAT_ARG {
	size_t		st_stat_len;
	struct stat*	st_stat;
};
#define _HCTL_FILE_LAST		199

/* Handle options - memory */
#define _HCTL_MEMORY_FIRST	200
#define HCTL_MEMORY_GET_INFO	(_HCTL_MEMORY_FIRST+0)	/* Retrieve information on memory handle */
struct HCTL_MEMORY_GET_INFO_ARG {
	void*		in_base;
	size_t		in_length;
};
#define _HCTL_MEMORY_LAST	299

/* Handle options - thread */
#define _HCTL_THREAD_FIRST	300
#define HCTL_THREAD_SUSPEND	(_HCTL_THREAD_FIRST+0)	/* Suspend a given thread */
#define HCTL_THREAD_RESUME	(_HCTL_THREAD_FIRST+1)	/* Resume a given thread */
#define _HCTL_THREAD_LAST 	399

#endif /* __SYS_HANDLE_OPTIONS_H__ */
