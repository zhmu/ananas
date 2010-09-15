#ifndef __SYS_MOUNT_H__
#define __SYS_MOUNT_H__

#define MNT_RDONLY	0x0001	/* read only */
#define MNT_NOWAIT	0x0002

typedef int fsid_t;

struct statfs {
	int	f_type;		/* filesystem type */
	int	f_bsize;	/* optimal block size */
	int	f_blocks;	/* total number of blocks */
	int	f_bfree;	/* free blocks */
	int	f_bavail;	/* available blocks (non-superuser) */
	int	f_files;	/* total number of file inodes */
	int	f_ffree;	/* total number of free inodes */
	fsid_t	f_fsid;		/* file system id */
	int	f_namelen;	/* maximum filename length */
	char*	f_mntfromname;
	char*	f_mntonname;
};

int getmntinfo(struct statfs** mntbufp, int flags);

#endif /* __SYS_MOUNT_H__ */
