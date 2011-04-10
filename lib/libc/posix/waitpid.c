#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/thread.h>
#include <sys/wait.h>
#include <_posix/error.h>
#include <_posix/handlemap.h>
#include <errno.h>

pid_t waitpid(pid_t pid, int *stat_loc, int options)
{
	errorcode_t err;
	void* handle = handlemap_deref(pid, HANDLEMAP_TYPE_PID);
	if (handle == NULL) {
		errno = ECHILD;
		return (pid_t)-1;
	}

	handle_event_t event = HANDLE_EVENT_ANY;
	handle_event_result_t result;
	err = sys_wait(handle, &event, &result);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return (pid_t)-1;
	}

	if (stat_loc != NULL) {
		if (event == THREAD_EVENT_EXIT) {
			/* Thread exited; like we wanted. Copy the status over */
			*stat_loc = (W_EXITED << 8) | (result & 0xff);
		}
	}
	/* We got all we needed from the thread; give it a proper burial */
	sys_destroy(handle);
	handlemap_free_entry(pid);
	return (pid_t)pid;
}
