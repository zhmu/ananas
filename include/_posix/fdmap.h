#ifndef __POSIX_FDMAP_H__
#define __POSIX_FDMAP_H__

#define FDMAP_SIZE	16

struct THREADINFO* ti;

void fdmap_init(struct THREADINFO* ti);
int fdmap_alloc_fd(void* handle);
void fdmap_free_fd(int fd);
void* fdmap_deref(int fd);
void fdmap_set_handle(int fd, void* handle);

#endif /* __POSIX_FDMAP_H__ */
