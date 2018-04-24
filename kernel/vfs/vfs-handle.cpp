#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/flags.h>
#include <ananas/handle-options.h>
#include "kernel/bio.h"
#include "kernel/handle.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"

TRACE_SETUP;

static Result
vfshandle_get_file(struct HANDLE* handle, struct VFS_FILE** out)
{
	if (handle->h_type != HANDLE_TYPE_FILE)
		return RESULT_MAKE_FAILURE(EBADF);

	struct VFS_FILE* file = &((struct HANDLE*)handle)->h_data.d_vfs_file;
	if (file->f_dentry == NULL && file->f_device == NULL)
		return RESULT_MAKE_FAILURE(EBADF);

	*out = file;
	return Result::Success();
}

Result
vfshandle_read(Thread* t, handleindex_t index, struct HANDLE* handle, void* buffer, size_t* size)
{
	struct VFS_FILE* file;

	RESULT_PROPAGATE_FAILURE(
		vfshandle_get_file(handle, &file)
	);

	return vfs_read(file, buffer, size);
}

Result
vfshandle_write(Thread* t, handleindex_t index, struct HANDLE* handle, const void* buffer, size_t* size)
{
	struct VFS_FILE* file;
	RESULT_PROPAGATE_FAILURE(
		vfshandle_get_file(handle, &file)
	);

	return vfs_write(file, buffer, size);
}

static Result
vfshandle_open(Thread* t, handleindex_t index, struct HANDLE* handle, const char* path, int flags, int mode)
{
	Process& proc = *t->t_process;

	/*
	 * If we could try to create the file, do so - if this fails, we'll attempt
	 * the ordinary open. This should have the advantage of eliminating a race
	 * condition.
	 */
	if (flags & O_CREAT) {
		/* Attempt to create the new file - if this works, we're all set */
		Result result = vfs_create(proc.p_cwd, &handle->h_data.d_vfs_file, path, mode);
		if (result.IsSuccess())
			return result;

		/*
		 * File could not be created; if we had to create the file, this is an
		 * error whatsoever, otherwise we'll report anything but 'file already
		 * exists' back to the caller.
		 */
		if ((flags & O_EXCL) || result.AsErrno() != EEXIST)
			return result;

		/* File can't be created - try opening it instead */
	}

	/* XXX do something more with mode / open */

	/* And open the path */
	TRACE(SYSCALL, INFO, "opening path '%s'", path);
	return vfs_open(&proc, path, proc.p_cwd, &handle->h_data.d_vfs_file);
}

static Result
vfshandle_free(Process& proc, struct HANDLE* handle)
{
	auto& file = handle->h_data.d_vfs_file;
	return vfs_close(&proc, &file);
}

static Result
vfshandle_unlink(Thread* t, handleindex_t index, struct HANDLE* handle)
{
	struct VFS_FILE* file;
	RESULT_PROPAGATE_FAILURE(
		vfshandle_get_file(handle, &file)
	);

	return vfs_unlink(file);
}

static Result
vfshandle_clone(Process& p_in, handleindex_t index_in, struct HANDLE* handle_in, struct CLONE_OPTIONS* opts, Process& proc_out, struct HANDLE** handle_out, handleindex_t index_out_min, handleindex_t* index_out)
{
	struct VFS_FILE* file;
	RESULT_PROPAGATE_FAILURE(
		vfshandle_get_file(handle_in, &file)
	);

	/*
	 * If the handle has a backing dentry reference, we have to increase it's
	 * reference count as we, too, depend on it. Closing the handle
	 * will release the dentry, which will remove it if needed.
	 */
	DEntry* dentry = file->f_dentry;
	if (dentry != nullptr)
		dentry_ref(*dentry);

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
