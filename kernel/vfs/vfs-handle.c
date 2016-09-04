#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/bio.h>
#include <ananas/flags.h>
#include <ananas/handle.h>
#include <ananas/handle-options.h>
#include <ananas/process.h>
#include <ananas/syscall.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>
#include <ananas/vm.h>
#include <ananas/lib.h>

TRACE_SETUP;

static errorcode_t
vfshandle_get_file(struct HANDLE* handle, struct VFS_FILE** out)
{
	if (handle->h_type != HANDLE_TYPE_FILE)
		return ANANAS_ERROR(BAD_HANDLE);

	struct VFS_FILE* file = &((struct HANDLE*)handle)->h_data.d_vfs_file;
	if (file->f_dentry == NULL && file->f_device == NULL)
		return ANANAS_ERROR(BAD_HANDLE);

	*out = file;
	return ANANAS_ERROR_OK;
}

errorcode_t
vfshandle_read(thread_t* t, handleindex_t index, struct HANDLE* handle, void* buffer, size_t* size)
{
	struct VFS_FILE* file;
	errorcode_t err = vfshandle_get_file(handle, &file);
	ANANAS_ERROR_RETURN(err);

	return vfs_read(file, buffer, size);
}

errorcode_t
vfshandle_write(thread_t* t, handleindex_t index, struct HANDLE* handle, const void* buffer, size_t* size)
{
	struct VFS_FILE* file;
	errorcode_t err = vfshandle_get_file(handle, &file);
	ANANAS_ERROR_RETURN(err);

	return vfs_write(file, buffer, size);
}

static errorcode_t
vfshandle_open(thread_t* t, handleindex_t index, struct HANDLE* handle, const char* path, int flags, int mode)
{
	process_t* proc = t->t_process;

	/*
	 * If we could try to create the file, do so - if this fails, we'll attempt
	 * the ordinary open. This should have the advantage of eliminating a race
	 * condition.
	 */
	if (flags & O_CREAT) {
		/* Attempt to create the new file - if this works, we're all set */
		errorcode_t err = vfs_create(proc->p_cwd, &handle->h_data.d_vfs_file, path, mode);
		if (err == ANANAS_ERROR_NONE)
			return err;

		/*
		 * File could not be created; if we had to create the file, this is an
		 * error whatsoever, otherwise we'll report anything but 'file already
		 * exists' back to the caller.
		 */
		if ((flags & O_EXCL) || ANANAS_ERROR_CODE(err) != ANANAS_ERROR_FILE_EXISTS)
			return err;

		/* File can't be created - try opening it instead */
	}

	/* XXX do something more with mode / open */

	/* And open the path */
	TRACE(SYSCALL, INFO, "opening path '%s'", path);
	return vfs_open(path, proc->p_cwd, &handle->h_data.d_vfs_file);
}

static errorcode_t
vfshandle_free(process_t* proc, struct HANDLE* handle)
{
	/* If we have a backing dentry, dereference it - this will free it if needed */
	struct DENTRY* dentry = handle->h_data.d_vfs_file.f_dentry;
	if (dentry != NULL)
		dentry_deref(dentry);
	return ANANAS_ERROR_OK;
}

static errorcode_t
vfshandle_unlink(thread_t* t, handleindex_t index, struct HANDLE* handle)
{
	struct VFS_FILE* file;
	errorcode_t err = vfshandle_get_file(handle, &file);
	ANANAS_ERROR_RETURN(err);

	return vfs_unlink(file);
}

static errorcode_t
vfshandle_clone(process_t* p_in, handleindex_t index_in, struct HANDLE* handle_in, struct CLONE_OPTIONS* opts, process_t* proc_out, struct HANDLE** handle_out, handleindex_t index_out_min, handleindex_t* index_out)
{
	struct VFS_FILE* file;
	errorcode_t err = vfshandle_get_file(handle_in, &file);
	ANANAS_ERROR_RETURN(err);

	/*
	 * If the handle has a backing dentry reference, we have to increase it's
	 * reference count as we, too, depend on it. Closing the handle
	 * will release the dentry, which will remove it if needed.
	 */
	struct DENTRY* dentry = file->f_dentry;
	if (dentry != NULL)
		dentry_ref(dentry);

	/* Now, just ordinarely clone the handle */
	return handle_clone_generic(handle_in, proc_out, handle_out, index_out_min, index_out);
}

struct HANDLE_OPS vfs_hops = {
	.hop_read = vfshandle_read,
	.hop_write = vfshandle_write,
	.hop_open = vfshandle_open,
	.hop_free = vfshandle_free,
	.hop_unlink = vfshandle_unlink,
	.hop_clone = vfshandle_clone,
};
HANDLE_TYPE(HANDLE_TYPE_FILE, "file", vfs_hops);

/* vim:set ts=2 sw=2: */
