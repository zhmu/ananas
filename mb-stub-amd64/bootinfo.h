#ifndef __BOOTINFO_H__
#define __BOOTINFO_H__

struct BOOTINFO;
struct RELOCATE_INFO;
struct MULTIBOOT;

struct BOOTINFO* create_bootinfo(const struct RELOCATE_INFO* ri_kernel, const struct MULTIBOOT* mb);

#endif /* __BOOTINFO_H__ */
