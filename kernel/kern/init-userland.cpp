#include "kernel/cmdline.h"
#include "kernel/exec.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/time.h"
#include "kernel/vfs/core.h"
#include "kernel-md/md.h"
#include "options.h"

namespace {

void
userinit_func(void*)
{
	/* We expect root=type:device here */
	const char* rootfs_arg = cmdline_get_string("root");
	if (rootfs_arg == NULL) {
		kprintf("'root' option not specified, not mounting a root filesystem\n");
		thread_exit(0);
	}

	char rootfs_type[64];
	const char* p = strchr(rootfs_arg, ':');
	if (p == NULL) {
		kprintf("cannot parse 'root' - expected type:device\n");
		thread_exit(0);
	}

	memcpy(rootfs_type, rootfs_arg, p - rootfs_arg);
	rootfs_type[p - rootfs_arg] = '\0';
	const char* rootfs = p + 1;

	kprintf("- Mounting / (type %s) from %s...", rootfs_type, rootfs);
	while(true) {
		if (auto result = vfs_mount(rootfs, "/", rootfs_type, NULL); result.IsSuccess())
			break;
		else
			kprintf(" failure %d, retrying...", result.AsStatusCode());

		delay(1000); // XXX we really need a sleep mechanism here
	}
	kprintf(" ok\n");

#ifdef FS_ANKHFS
	kprintf("- Mounting /ankh...");
	if (auto result = vfs_mount(nullptr, "/ankh", "ankhfs", nullptr); result.IsSuccess())
		kprintf(" success\n");
	else
		kprintf(" error %d\n", result.AsStatusCode());
#endif

	// Now it makes sense to try to load init
	const char* init_path = cmdline_get_string("init");
	if (init_path == NULL || init_path[0] == '\0') {
		kprintf("'init' option not specified, not starting init\n");
		thread_exit(0);
	}

	Process* proc;
	if (auto result = process_alloc(NULL, proc); result.IsFailure()) {
		kprintf("couldn't create process, %i\n", result.AsStatusCode());
		thread_exit(0);
	}

	Thread* t;
	{
		Result result = thread_alloc(*proc, t, "init", THREAD_ALLOC_DEFAULT);
		process_deref(*proc); /* 't' should have a ref, we don't need it anymore */
		if (result.IsFailure()) {
			kprintf("couldn't create thread, %i\n", result.AsStatusCode());
			thread_exit(0);
		}
	}

	kprintf("- Lauching init from %s...", init_path);
	struct VFS_FILE file;
	if (auto result = vfs_open(init_path, proc->p_cwd, &file); result.IsFailure()) {
		kprintf("couldn't open init executable, %i\n", result.AsStatusCode());
		thread_exit(0);
	}

	const char args[] = "init\0\0";
	const char env[] = "OS=Ananas\0USER=root\0\0";
	process_set_args(*proc, args, sizeof(args));
	process_set_environment(*proc, env, sizeof(env));

	addr_t exec_addr;
	register_t exec_arg;
	if (auto result = exec_load(*proc->p_vmspace, *file.f_dentry, &exec_addr, &exec_arg); result.IsSuccess()) {
		kprintf(" ok\n");
		md::thread::SetupPostExec(*t, exec_addr, exec_arg);
		t->Resume();
	} else {
		kprintf(" fail - error %i\n", result.AsStatusCode());
	}
	vfs_close(&file);
	thread_exit(0);
}

Thread userinit_thread;

Result
init_userland()
{
	kthread_init(userinit_thread, "user-init", &userinit_func, nullptr);
	userinit_thread.Resume();
	return Result::Success();
}

} // unnamed namespace

INIT_FUNCTION(init_userland, SUBSYSTEM_SCHEDULER, ORDER_LAST);

/* vim:set ts=2 sw=2: */
