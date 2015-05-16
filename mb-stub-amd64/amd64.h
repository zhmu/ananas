#ifndef __AMD64_H__
#define __AMD64_H__

#include "types.h"

struct BOOTINFO;

void amd64_exec(uint64_t entry, struct BOOTINFO* bootinfo) __attribute__((noreturn));

#endif /* __AMD64_H__ */
