#ifndef __POSIX_HANDLEMAP_H__
#define __POSIX_HANDLEMAP_H__

#include <ananas/types.h>

#define HANDLEMAP_SIZE	16

struct HANDLEMAP_ENTRY {
#define HANDLEMAP_TYPE_ANY	-1	/* used for deref */
#define HANDLEMAP_TYPE_UNUSED	0
#define HANDLEMAP_TYPE_FD	1	/* file descriptor */
#define HANDLEMAP_TYPE_PID	2	/* process id */
	int	hm_type;
	void*	hm_handle;
};

struct stat;

typedef ssize_t (*handlemap_read_fn)(int idx, void* handle, void* buf, size_t len);
typedef ssize_t (*handlemap_write_fn)(int idx, void* handle, const void* buf, size_t len);
typedef void (*handlemap_close_fn)(int idx, void* handle);
typedef off_t (*handlemap_seek_fn)(int idx, void* handle, off_t offset, int whence);
typedef int (*handlemap_stat_fn)(int idx, void* handle, struct stat* buf);

struct HANDLEMAP_OPS {
	handlemap_read_fn hop_read;
	handlemap_write_fn hop_write;
	handlemap_close_fn hop_close;
	handlemap_seek_fn hop_seek;
	handlemap_stat_fn hop_stat;
};

struct THREADINFO;

void handlemap_init(struct THREADINFO* ti);
void handlemap_reinit(struct THREADINFO* ti);
int handlemap_alloc_entry(int type, void* handle);
int handlemap_get_type(int idx);
void handlemap_free_entry(int idx);
struct HANDLEMAP_OPS* handlemap_get_ops(int idx);
void* handlemap_deref(int idx, int type);
void handlemap_set_handle(int idx, void* handle);
void handlemap_set_entry(int idx, int type, void* handle);

#endif /* __POSIX_HANDLEMAP_H__ */
