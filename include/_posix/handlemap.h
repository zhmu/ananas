#ifndef __POSIX_HANDLEMAP_H__
#define __POSIX_HANDLEMAP_H__

#define HANDLEMAP_SIZE	16

struct HANDLEMAP_ENTRY {
#define HANDLEMAP_TYPE_ANY	-1	/* used for deref */
#define HANDLEMAP_TYPE_UNUSED	0
#define HANDLEMAP_TYPE_FD	1	/* file descriptor */
#define HANDLEMAP_TYPE_PID	2	/* process id */
	int	hm_type;
	void*	hm_handle;
};

struct THREADINFO;

void handlemap_init(struct THREADINFO* ti);
void handlemap_reinit(struct THREADINFO* ti);
int handlemap_alloc_entry(int type, void* handle);
void handlemap_free_entry(int idx);
void* handlemap_deref(int idx, int type);
void handlemap_set_handle(int idx, void* handle);
void handlemap_set_entry(int idx, int type, void* handle);

#endif /* __POSIX_HANDLEMAP_H__ */
