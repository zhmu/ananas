#ifndef __ANANAS_STAT_H__
#define __ANANAS_STAT_H__

#include <machine/_types.h>
#include <ananas/_types/mode.h>
#include <ananas/_types/dev.h>
#include <ananas/_types/uid.h>
#include <ananas/_types/gid.h>
#include <ananas/_types/ino.h>
#include <ananas/_types/off.h>
#include <ananas/_types/nlink.h>
#include <ananas/_types/blksize.h>
#include <ananas/_types/blkcnt.h>
#include <ananas/_types/time.h>

#define S_IFMT 0xf000 /* File type */
#define S_IFSOCK 0xc000
#define S_IFLNK 0xa000
#define S_IFREG 0x8000
#define S_IFBLK 0x6000
#define S_IFDIR 0x4000
#define S_IFCHR 0x2000
#define S_IFIFO 0x1000

#define S_ISBLK(m) (((m)&S_IFMT) == S_IFBLK)
#define S_ISCHR(m) (((m)&S_IFMT) == S_IFCHR)
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) (((m)&S_IFMT) == S_IFIFO)
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m)&S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m)&S_IFMT) == S_IFSOCK)

#define S_TYPEISMQ(m) 0  /* XXX unsupported */
#define S_TYPEISSEM(m) 0 /* XXX unsupported */
#define S_TYPEISSHM(m) 0 /* XXX unsupported */

/* File permission bits */
#define S_ISUID 0x0800 /* Set UID */
#define S_ISGID 0x0400 /* Set GID */
#define S_ISVTX 0x0200 /* Delete restriction (directories) */

#define S_IRWXU 0x01c0 /* Read, write, execute/search (owner) */
#define S_IRUSR 0x0100
#define S_IWUSR 0x0080
#define S_IXUSR 0x0040
#define S_IRWXG 0x0038 /* Read, write, execute/search (group) */
#define S_IRGRP 0x0020
#define S_IWGRP 0x0010
#define S_IXGRP 0x0008
#define S_IRXWXO 0x0007 /* Read, write, execute/search (others) */
#define S_IROTH 0x0004
#define S_IWOTH 0x0002
#define S_IXOTH 0x0001

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

#endif /* __ANANAS_STAT_H__ */
