#ifndef __SYS_MOUNT_H__
#define __SYS_MOUNT_H__

#include <sys/cdefs.h>

#define MNT_RDONLY 0x0001 /* read only */
#define MNT_NOWAIT 0x0002

#define MFSNAMELEN 16 /* length of type name including \0 */
#define MNAMELEN 88   /* name buffer length */

typedef int fsid_t;

struct statfs {
    int f_type;    /* filesystem type */
    int f_flags;   /* filesystem flags */
    int f_bsize;   /* optimal block size */
    int f_blocks;  /* total number of blocks */
    int f_bfree;   /* free blocks */
    int f_bavail;  /* available blocks (non-superuser) */
    int f_files;   /* total number of file inodes */
    int f_ffree;   /* total number of free inodes */
    fsid_t f_fsid; /* file system id */
    int f_namelen; /* maximum filename length */
    char f_fstypename[MFSNAMELEN];
    char f_mntfromname[MNAMELEN];
    char f_mntonname[MNAMELEN];
};

__BEGIN_DECLS

int getmntinfo(struct statfs** mntbufp, int flags);

int statfs(const char* path, struct statfs* buf);
int fstatfs(int fildes, struct statfs* buf);

/* Ananas extensions */
int mount(const char* type, const char* source, const char* dir, int flags);
int unmount(const char* dir, int flags);

__END_DECLS

#endif /* __SYS_MOUNT_H__ */
