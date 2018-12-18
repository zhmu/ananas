#ifndef _PDCLIB_PWD_H_
#define _PDCLIB_PWD_H_

#include <machine/_types.h>
#include <ananas/_types/uid.h>
#include <ananas/_types/gid.h>

#ifdef __cplusplus
extern "C" {
#endif

struct passwd {
    char* pw_name;
    uid_t pw_uid;
    gid_t pw_gid;
    char* pw_dir;
    char* pw_shell;
};

struct passwd* getpwnam(const char*);
struct passwd* getpwuid(uid_t);

void endpwent();
struct passwd* getpwent();
void setpwent();

#ifdef __cplusplus
}
#endif

#endif
