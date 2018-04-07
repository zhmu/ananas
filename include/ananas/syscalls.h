#include <ananas/types.h>
#include <ananas/syscall-vmops.h>
#include <ananas/stat.h>

struct utimbuf;
struct Thread;
struct sigaction;

#ifdef KERNEL
class Result;
#else
typedef statuscode_t Result;
#endif

#include <_gen/syscalls.h>
