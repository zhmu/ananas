#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/process.h>
#include <ananas/syscall.h>
#include <ananas/trace.h>
#include <ananas/vm.h>

TRACE_SETUP;

errorcode_t
sys_fchdir(thread_t* t, handleindex_t index)
{
	TRACE(SYSCALL, FUNC, "t=%p, index=%d'", t, index);
	process_t* proc = t->t_process;
	struct DENTRY* cwd = proc->p_cwd;

	/* Get the handle */
	struct HANDLE* h;
	errorcode_t err = syscall_get_handle(t, index, &h);
	ANANAS_ERROR_RETURN(err);

	if (h->h_type != HANDLE_TYPE_FILE)
		return ANANAS_ERROR(BAD_HANDLE);

        struct VFS_FILE* file = &h->h_data.d_vfs_file;
	struct DENTRY* new_cwd = file->f_dentry;
	dentry_ref(new_cwd);
	proc->p_cwd = new_cwd;
	dentry_deref(cwd);

	return err;
}
