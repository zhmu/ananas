#ifndef ANANAS_VFS_DIRENT_H
#define ANANAS_VFS_DIRENT_H

/*
 * Directory entry, as returned by the kernel. The fsop length is stored
 * for easy iteration on the caller side.
 */
struct VFS_DIRENT {
	uint32_t	de_flags;		/* Flags */
	uint8_t		de_fsop_length;		/* Length of identifier */
	uint8_t		de_name_length;		/* Length of name */
	char		de_fsop[1];		/* Identifier */
	/*
	 * de_name will be stored directly after the fsop.
	 */
};
#define DE_LENGTH(x) (sizeof(struct VFS_DIRENT) + (x)->de_fsop_length + (x)->de_name_length)
#define DE_FSOP(x) (&((x)->de_fsop))
#define DE_NAME(x) (&((x)->de_fsop[(x)->de_fsop_length]))


#endif // ANANAS_VFS_DIRENT_H
