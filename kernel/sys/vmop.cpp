#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/syscall-vmops.h>
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

TRACE_SETUP;

static errorcode_t
sys_vmop_map(Thread* curthread, struct VMOP_OPTIONS* vo)
{
	if (vo->vo_len == 0)
		return ANANAS_ERROR(BAD_LENGTH);
	if ((vo->vo_flags & (VMOP_FLAG_PRIVATE | VMOP_FLAG_SHARED)) == 0)
		return ANANAS_ERROR(BAD_FLAG);

	int vm_flags = VM_FLAG_USER | VM_FLAG_FAULT;
	if (vo->vo_flags & VMOP_FLAG_READ)
		vm_flags |= VM_FLAG_READ;
	if (vo->vo_flags & VMOP_FLAG_WRITE)
		vm_flags |= VM_FLAG_WRITE;
	if (vo->vo_flags & VMOP_FLAG_EXECUTE)
		vm_flags |= VM_FLAG_EXECUTE;
	if (vo->vo_flags & VMOP_FLAG_PRIVATE)
		vm_flags |= VM_FLAG_PRIVATE;

	VMSpace& vs = *curthread->t_process->p_vmspace;
	addr_t dest_addr = reinterpret_cast<addr_t>(vo->vo_addr);
	if ((vo->vo_flags & VMOP_FLAG_FIXED) == 0)
		dest_addr = vmspace_determine_va(vs, vo->vo_len);

	VMArea* va;
	errorcode_t err;
	if (vo->vo_flags & VMOP_FLAG_HANDLE) {
		struct HANDLE* h;
		err = syscall_get_handle(*curthread, vo->vo_handle, &h);
		ANANAS_ERROR_RETURN(err);

		if (h->h_type != HANDLE_TYPE_FILE)
			return ANANAS_ERROR(BAD_HANDLE);
		DEntry* dentry = h->h_data.d_vfs_file.f_dentry;
		if (dentry == nullptr)
			return ANANAS_ERROR(BAD_HANDLE);

		err = vmspace_mapto_dentry(vs, dest_addr, vo->vo_len, *dentry, vo->vo_offset, vo->vo_len, vm_flags, va);
	} else {
		err = vmspace_mapto(vs, dest_addr, vo->vo_len, vm_flags, va);
	}
	ANANAS_ERROR_RETURN(err);

	vo->vo_addr = (void*)va->va_virt;
	vo->vo_len = va->va_len;
	return ananas_success();
}

static errorcode_t
sys_vmop_unmap(ARG_CURTHREAD struct VMOP_OPTIONS* vo)
{
	/* XXX implement me */
	return ANANAS_ERROR(BAD_OPERATION);
}

errorcode_t
sys_vmop(ARG_CURTHREAD struct VMOP_OPTIONS* opts)
{
	TRACE(SYSCALL, FUNC, "t=%p, opts=%p", curthread, opts);
	errorcode_t err;

	/* Opbtain options */
	struct VMOP_OPTIONS* vmop_opts;
	err = syscall_map_buffer(*curthread, opts, sizeof(*vmop_opts), VM_FLAG_READ | VM_FLAG_WRITE, (void**)&vmop_opts);
	ANANAS_ERROR_RETURN(err);
	if (vmop_opts->vo_size != sizeof(*vmop_opts))
		return ANANAS_ERROR(BAD_LENGTH);

	switch(vmop_opts->vo_op) {
		case OP_MAP:
			return sys_vmop_map(curthread, vmop_opts);
		case OP_UNMAP:
			return sys_vmop_unmap(curthread, vmop_opts);
		default:
			return ANANAS_ERROR(BAD_OPERATION);
	}

	/* NOTREACHED */
}
