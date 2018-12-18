#ifndef _PDCLIB_GRP_H_
#define _PDCLIB_GRP_H_

#include <machine/_types.h>
#include <ananas/_types/gid.h>

#ifdef __cplusplus
extern "C" {
#endif

struct group {
    char* gr_name;
    gid_t gr_gid;
    char** gr_mem;
};

struct group* getgrgid(gid_t);
struct group* getgrnam(const char*);

struct group* getgrent();
void endgrent();
void setgrent();

#ifdef __cplusplus
}
#endif

#endif
