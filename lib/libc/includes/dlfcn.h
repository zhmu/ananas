#ifndef ANANAS_DLFCN_H
#define ANANAS_DLFCN_H

#include <sys/cdefs.h>

__BEGIN_DECLS

void* dlopen(const char* file, int mode);
void* dlsym(void* handle, const char* name);
int dlclose(void* handle);
char* dlerror(void);

/* Extensions */
typedef struct dl_info {
    const char* dli_fname;
    void* dli_fbase;
    const char* dli_sname;
    void* dli_saddr;
} Dl_info;

int dladdr(const void* addr, Dl_info* info);

__END_DECLS

#endif /* ANANAS_DLFCN_H */
