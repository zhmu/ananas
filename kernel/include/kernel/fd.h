#ifndef FD_H
#define FD_H

#include <ananas/util/list.h>
#include "kernel/lock.h"
#include "kernel/vfs/types.h"

#define FD_TYPE_ANY	-1
#define FD_TYPE_UNUSED	0
#define FD_TYPE_FILE	1

struct Process;
struct Thread;
struct FDOperations;
class Result;

struct FD : util::List<FD>::NodePtr {
	int fd_type = 0;			/* one of FD_TYPE_... */
	int fd_flags = 0;			/* flags */
	Process* fd_process = nullptr;		/* owning process */
	Mutex fd_mutex{"fd"};			/* mutex guarding the descriptor */
	const FDOperations* fd_ops = nullptr;	/* descriptor operations */

	// Descriptor-specific data
	union {
		struct VFS_FILE d_vfs_file;
	} fd_data{};

	Result Close();
};

// Descriptor operations map almost directly to the syscalls invoked on them.
struct CLONE_OPTIONS;
typedef Result (*fd_read_fn)(Thread* thread, fdindex_t index, FD& fd, void* buf, size_t* len);
typedef Result (*fd_write_fn)(Thread* thread, fdindex_t index, FD& fd, const void* buf, size_t* len);
typedef Result (*fd_open_fn)(Thread* thread, fdindex_t index, FD& fd, const char* path, int flags, int mode);
typedef Result (*fd_free_fn)(Process& proc, FD& fd);
typedef Result (*fd_unlink_fn)(Thread* thread, fdindex_t index, FD& fd);
typedef Result (*fd_clone_fn)(Process& proc_in, fdindex_t index, FD& fd_in, struct CLONE_OPTIONS* opts, Process& proc_out, FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out);
typedef Result (*fd_ioctl_fn)(Thread* thread, fdindex_t index, FD& fd, unsigned long request, void* args[]);

struct FDOperations {
	fd_read_fn d_read;
	fd_write_fn d_write;
	fd_open_fn d_open;
	fd_free_fn d_free;
	fd_unlink_fn d_unlink;
	fd_clone_fn d_clone;
	fd_ioctl_fn d_ioctl;
};

/* Registration of descriptor types */
struct FDType : util::List<FDType>::NodePtr {
	FDType(const char* name, int id, FDOperations& fdops)
	 : ft_name(name), ft_id(id), ft_ops(fdops)
	{
	}
	const char* ft_name;
	const int ft_id;
	const FDOperations& ft_ops;
};

namespace fd {

void Initialize();
Result Allocate(int type, Process& proc, fdindex_t index_from, FD*& fd_out, fdindex_t& index_out);
Result Lookup(Process& proc, fdindex_t index, int type, FD*& fd_out);
Result FreeByIndex(Process& proc, fdindex_t index);
Result Clone(Process& proc_in, fdindex_t index_in, struct CLONE_OPTIONS* opts, Process& proc_out, FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out);

// Only to be used from descriptor implementation code
Result CloneGeneric(FD& fd_in, Process& proc_out, FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out);
void RegisterType(FDType& ft);
void UnregisterType(FDType& ft);

}

#define FD_TYPE(id, name, ops) \
	static FDType ft_##id(name, id, ops); \
	static Result register_##id() { \
		fd::RegisterType(ft_##id); \
		return Result::Success(); \
	}; \
	static Result unregister_##id() { \
		fd::UnregisterType(ft_##id); \
		return Result::Success(); \
	}; \
	INIT_FUNCTION(register_##id, SUBSYSTEM_HANDLE, ORDER_SECOND); \
	EXIT_FUNCTION(unregister_##id)

#endif /* FD_H */
