#include <ananas/error.h>
#include "kernel/cmdline.h"
#include "kernel/exec.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/time.h"
#include "kernel/vfs/core.h"
#include "options.h"

namespace {

void
userinit_func(void*)
{
	errorcode_t err;

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
		err = vfs_mount(rootfs, "/", rootfs_type, NULL);
		if (ananas_is_success(err))
			break;

		delay(1000); // XXX we really need a sleep mechanism here
	}
	kprintf(" ok\n");

#ifdef FS_ANKHFS
	kprintf("- Mounting /ankh...");
	err = vfs_mount(nullptr, "/ankh", "ankhfs", nullptr);
	if (ananas_is_failure(err))
		kprintf(" error %d\n", err);
	else
		kprintf(" success\n", err);
#endif

	// Now it makes sense to try to load init
	const char* init_path = cmdline_get_string("init");
	if (init_path == NULL || init_path[0] == '\0') {
		kprintf("'init' option not specified, not starting init\n");
		thread_exit(0);
	}

	process_t* proc;
	err = process_alloc(NULL, &proc);
	if (ananas_is_failure(err)) {
		kprintf("couldn't create process, %i\n", err);
		thread_exit(0);
	}

	thread_t* t;
	err = thread_alloc(proc, &t, "init", THREAD_ALLOC_DEFAULT);
	process_deref(proc); /* 't' should have a ref, we don't need it anymore */
	if (ananas_is_failure(err)) {
		kprintf("couldn't create thread, %i\n", err);
		thread_exit(0);
	}

	kprintf("- Lauching init from %s...", init_path);
	struct VFS_FILE file;
	err = vfs_open(init_path, proc->p_cwd, &file);
	if (ananas_is_failure(err)) {
		kprintf("couldn't open init executable, %i\n", err);
		thread_exit(0);
	}

	const char args[] = "init\0\0";
	const char env[] = "OS=Ananas\0USER=root\0\0";
	process_set_args(proc, args, sizeof(args));
	process_set_environment(proc, env, sizeof(env));

	addr_t exec_addr;
	register_t exec_arg;
	err = exec_load(*proc->p_vmspace, file.f_dentry, &exec_addr, &exec_arg);
	if (ananas_is_success(err)) {
		kprintf(" ok\n");
		md_setup_post_exec(t, exec_addr, exec_arg);
		thread_resume(t);
	} else {
		kprintf(" fail - error %i\n", err);
	}
	vfs_close(&file);
	thread_exit(0);
}

thread_t userinit_thread;

errorcode_t
init_userland()
{
	kthread_init(&userinit_thread, "user-init", &userinit_func, nullptr);
	thread_resume(&userinit_thread);
	return ananas_success();
}

} // unnamed namespace

INIT_FUNCTION(init_userland, SUBSYSTEM_SCHEDULER, ORDER_LAST);

/* vim:set ts=2 sw=2: */
