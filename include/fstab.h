#ifndef __FSTAB_H__
#define __FSTAB_H__

#include <sys/cdefs.h>

struct fstab {
	char*		fs_spec;	/* block device name */
	char*		fs_file;	/* mount point */
	char*		fs_vfstype;	/* file-system type */
	char*	 	fs_mntops;	/* mount options */
	const char*	fs_type;	/* FSTAB_* from fs_mntops */
	int		fs_freq;	/* dump frequency, in days */
	int		fs_passno;	/* pass number in parallel dump */
};

__BEGIN_DECLS

void endfsend(void);
struct fstab* getfsent(void);
int setfsent();

__END_DECLS

#endif /* __FSTAB_H__ */
