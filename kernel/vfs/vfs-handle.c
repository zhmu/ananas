#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/bio.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/syscall.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>
#include <ananas/lib.h>

TRACE_SETUP;

static errorcode_t
vfshandle_get_file(struct HANDLE* handle, struct VFS_FILE** out)
{
	if (handle->h_type != HANDLE_TYPE_FILE)
		return ANANAS_ERROR(BAD_HANDLE);

	struct VFS_FILE* file = &((struct HANDLE*)handle)->h_data.d_vfs_file;
	if (file->f_inode == NULL && file->f_device == NULL)
		return ANANAS_ERROR(BAD_HANDLE);

	*out = file;
	return ANANAS_ERROR_OK;
}

errorcode_t
vfshandle_read(thread_t* thread, struct HANDLE* handle, void* buffer, size_t* size)
{
	struct VFS_FILE* file;
	errorcode_t err = vfshandle_get_file(handle, &file);
	ANANAS_ERROR_RETURN(err);

	return vfs_read(file, buffer, size);
}

errorcode_t
vfshandle_write(thread_t* thread, struct HANDLE* handle, const void* buffer, size_t* size)
{
	struct VFS_FILE* file;
	errorcode_t err = vfshandle_get_file(handle, &file);
	ANANAS_ERROR_RETURN(err);

	return vfs_write(file, buffer, size);
}

static errorcode_t
vfshandle_open(thread_t* thread, struct HANDLE* handle, struct OPEN_OPTIONS* opts)
{
	const char* userpath = opts->op_path; /* safe because sys_open maps it */

	/*
	 * If we could try to create the file, do so - if this fails, we'll attempt
	 * the ordinary open. This should have the advantage of eliminating a race
	 * condition.
	 */
	if (opts->op_mode & OPEN_MODE_CREATE) {
		/* Attempt to create the new file - if this works, we're all set */
		errorcode_t err = vfs_create(thread->t_path_handle->h_data.d_vfs_file.f_inode, &handle->h_data.d_vfs_file, userpath, opts->op_createmode);
		if (err == ANANAS_ERROR_NONE)
			return err;

		/*
		 * File could not be created; if we had to create the file, this is an
		 * error whatsoever, otherwise we'll report anything but 'file already
		 * exists' back to the caller.
		 */
		if ((opts->op_mode & OPEN_MODE_EXCLUSIVE) || ANANAS_ERROR_CODE(err) != ANANAS_ERROR_FILE_EXISTS)
			return err;

		/* File can't be created - try opening it instead */
	}

	/* And open the path */
	TRACE(SYSCALL, INFO, "opening userpath '%s'", userpath);
	return vfs_open(userpath, thread->t_path_handle->h_data.d_vfs_file.f_inode, &handle->h_data.d_vfs_file);
}

static errorcode_t
vfshandle_free(thread_t* thread, struct HANDLE* handle)
{
	/* If we have a backing inode, dereference it - this will free it if needed */
	struct VFS_INODE* inode = handle->h_data.d_vfs_file.f_inode;
	if (inode != NULL)
		vfs_deref_inode(inode);
	return ANANAS_ERROR_OK;
}

static errorcode_t
vfshandle_unlink(thread_t* thread, struct HANDLE* handle)
{
	return ANANAS_ERROR(BAD_OPERATION);
}

static errorcode_t
vfshandle_create(thread_t* thread, struct HANDLE* handle, struct CREATE_OPTIONS* opts)
{
	/* Fetch the new path name */
	const char* path;
	errorcode_t err = syscall_map_string(thread, opts->cr_path, &path);
	ANANAS_ERROR_RETURN(err);

	/* Attempt to create the new file */
	return vfs_create(thread->t_path_handle->h_data.d_vfs_file.f_inode, &handle->h_data.d_vfs_file, path, opts->cr_mode);
}

static errorcode_t
vfshandle_control(thread_t* thread, struct HANDLE* handle, unsigned int op, void* arg, size_t len)
{
	errorcode_t err;

	/* Grab the file handle - we'll always need it as we're doing files */
	struct VFS_FILE* file;
	err = vfshandle_get_file(handle, &file);
	ANANAS_ERROR_RETURN(err);

	/* If this is a device-specific operation, deal with it */
	if (op >= _HCTL_DEVICE_FIRST && op <= _HCTL_DEVICE_LAST) {
		/* See if it's device-backed; it must be for this handlectl to work */
		if (file->f_device == NULL)
			return ANANAS_ERROR(NO_DEVICE);

		/* Device driver must implement the devctl call as we're about to call it */
		if (file->f_device->driver->drv_devctl == NULL)
			return ANANAS_ERROR(BAD_OPERATION);

		/*
		 * Note that arg/len are already filled out at this point, so we can just
		 * call the devctl
		 */
		return file->f_device->driver->drv_devctl(file->f_device, thread, op, arg, len);
	}

	switch(op) {
		case HCTL_FILE_SETCWD: {
			/* Ensure we are dealing with a directory here */
			if (!S_ISDIR(file->f_inode->i_sb.st_mode))
				return ANANAS_ERROR(NOT_A_DIRECTORY);

			/* XXX We should lock the thread? */

#if 0
			/*
			 * The inode must be owned by the thread already, so we could just hand
			 * it over without dealing with the reference count. However, closing the
			 * handle (as we don't want the thread to mess with it anymore) is the
			 * safest way to continue - so we just continue by referencing the inode
			 * and then freeing the handle (yes, this is a kludge).
			 *
			 * XXX Why isn't this necessary?
			 */
			vfs_ref_inode(file->inode);
#endif

			/* Disown the previous handle; it is no longer of concern */
			handle_free(thread->t_path_handle);

			/* And update the handle */
			thread->t_path_handle = handle;
			return ANANAS_ERROR_OK;
		}
		case HCTL_FILE_SEEK: {
			/* Ensure we understand the whence */
			struct HCTL_SEEK_ARG* se = arg;
			if (arg == NULL)
				return ANANAS_ERROR(BAD_ADDRESS);
			if (len != sizeof(*se))
				return ANANAS_ERROR(BAD_LENGTH);
			if (se->se_whence != HCTL_SEEK_WHENCE_SET && se->se_whence != HCTL_SEEK_WHENCE_CUR && se->se_whence != HCTL_SEEK_WHENCE_END)
				return ANANAS_ERROR(BAD_FLAG);
			off_t offset;
			err = syscall_fetch_offset(thread, se->se_offs, &offset);
			ANANAS_ERROR_RETURN(err);

			/* Update the offset */
			switch(se->se_whence) {
				case HCTL_SEEK_WHENCE_SET:
					break;
				case HCTL_SEEK_WHENCE_CUR:
					offset = file->f_offset + offset;
					break;
				case HCTL_SEEK_WHENCE_END:
					offset = file->f_inode->i_sb.st_size - offset;
					break;
			}
			if (offset < 0)
				return ANANAS_ERROR(BAD_RANGE);
			if (offset > file->f_inode->i_sb.st_size) {
				/* File needs to be grown to accommodate for this offset */
				err = vfs_grow(file, offset);
				ANANAS_ERROR_RETURN(err);
			}
			err = syscall_set_offset(thread, se->se_offs, offset);
			ANANAS_ERROR_RETURN(err);
			file->f_offset = offset;
			return ANANAS_ERROR_OK;
		}
		case HCTL_FILE_STAT: {
			/* Ensure we understand the arguments */
			struct HCTL_STAT_ARG* st = arg;
			if (arg == NULL)
				return ANANAS_ERROR(BAD_ADDRESS);
			if (len != sizeof(*st) || st->st_stat_len != sizeof(struct stat))
				return ANANAS_ERROR(BAD_LENGTH);

			void* dest;
			err = syscall_map_buffer(thread, st->st_stat, sizeof(struct stat), THREAD_MAP_WRITE, &dest);
			ANANAS_ERROR_RETURN(err);

			if (file->f_inode != NULL) {
				/* Copy the data and we're done */
				memcpy(dest, &file->f_inode->i_sb, sizeof(struct stat));
			} else {
				/* First of all, start by filling with defaults */
				struct stat* st = dest;
				st->st_dev     = (dev_t)(uintptr_t)file->f_device;
				st->st_ino     = (ino_t)0;
				st->st_mode    = 0666;
				st->st_nlink   = 1;
				st->st_uid     = 0;
				st->st_gid     = 0;
				st->st_rdev    = (dev_t)(uintptr_t)file->f_device;
				st->st_size    = 0;
				st->st_atime   = 0;
				st->st_mtime   = 0;
				st->st_ctime   = 0;
				st->st_blksize = BIO_SECTOR_SIZE;
				if (file->f_device->driver->drv_stat != NULL) {
					/* Allow the device driver to update */
					err = file->f_device->driver->drv_stat(file->f_device, st);
				}
			}
			return err;
		}
		default:
			/* What's this? */
			return ANANAS_ERROR(BAD_SYSCALL);
	}

	/* NOTREACHED */
}

static errorcode_t
vfshandle_clone(thread_t* thread, struct HANDLE* handle, struct HANDLE** result)
{
	struct VFS_FILE* file;
	errorcode_t err = vfshandle_get_file(handle, &file);
	ANANAS_ERROR_RETURN(err);

	/*
	 * If the handle has a backing inode reference, we have to increase it's
	 * reference count as we, too, depend on it. Closing the handle
	 * will release the inode, which will remove it if needed.
	 */
	struct VFS_INODE* inode = file->f_inode;
	if (inode != NULL)
		vfs_ref_inode(inode);

	/* Now, just ordinarely clone the handle */
	return handle_clone_generic(thread, handle, result);
}

static errorcode_t
vfshandle_summon(thread_t* thread, struct HANDLE* handle, struct SUMMON_OPTIONS* opts, struct HANDLE** out)
{
	struct VFS_FILE* file;
	errorcode_t err = vfshandle_get_file(handle, &file);
	ANANAS_ERROR_RETURN(err);

	/* Create a new thread */
	thread_t* newthread = NULL;
	err = thread_alloc(thread, &newthread);
	ANANAS_ERROR_RETURN(err);

	/*
	 * Create a reference to the new thread's handle to give to our parent; we are not
	 * the owner of the thread handle, so we can't directly return it.
	 */
	struct HANDLE* ref_handle = NULL;
	err = handle_create_ref(thread, newthread->t_thread_handle, &ref_handle);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	err = syscall_set_handle(thread, (void**)out, ref_handle);
	if (err != ANANAS_ERROR_NONE)
		goto fail;

	/* Ask the VFS to summon the file over our thread */
	err = vfs_summon(file, newthread);
	if (err != ANANAS_ERROR_NONE) {
		/* Failure - no option but to kill the thread */
		goto fail;
	}

	/* If arguments are given, handle them */
	if (opts->su_args != NULL) {
		const char* arg;
		err = syscall_map_buffer(thread, opts->su_args, opts->su_args_len, THREAD_MAP_READ, (void**)&arg);
		if (err == ANANAS_ERROR_NONE)
			err = thread_set_args(newthread, arg, opts->su_args_len);
		if (err != ANANAS_ERROR_NONE)
			goto fail;
	}

	/* If an environment is given, handle it */
	if (opts->su_env != NULL) {
		const char* env;
		err = syscall_map_buffer(thread, opts->su_env, opts->su_env_len, THREAD_MAP_READ, (void**)&env);
		if (err == ANANAS_ERROR_NONE)
			err = thread_set_environment(newthread, env, opts->su_env_len);
		if (err != ANANAS_ERROR_NONE)
			goto fail;
	}

	/* If we need to resume the thread, do so */
	if (opts->su_flags & SUMMON_FLAG_RUNNING)
		thread_resume(newthread);

	return ANANAS_ERROR_OK;

fail:
	if (ref_handle != NULL)
		handle_free(ref_handle);
	if (newthread != NULL) {
		thread_free(newthread);
		thread_destroy(newthread);
	}
	return err;
}

struct HANDLE_OPS vfs_hops = {
	.hop_read = vfshandle_read,
	.hop_write = vfshandle_write,
	.hop_open = vfshandle_open,
	.hop_free = vfshandle_free,
	.hop_unlink = vfshandle_unlink,
	.hop_create = vfshandle_create,
	.hop_control = vfshandle_control,
	.hop_clone = vfshandle_clone,
	.hop_summon = vfshandle_summon,
};
HANDLE_TYPE(HANDLE_TYPE_FILE, "file", vfs_hops);

/* vim:set ts=2 sw=2: */
