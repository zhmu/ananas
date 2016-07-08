#ifndef __I386_H__
#define __I386_H__

#include "types.h"

struct BOOTINFO;

void i386_exec(uint64_t entry, struct BOOTINFO* bootinfo) __attribute__((noreturn));

#endif /* __I386_H__ */
