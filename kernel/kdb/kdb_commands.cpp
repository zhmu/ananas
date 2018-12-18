#include <ananas/types.h>
#include <ananas/bootinfo.h>
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel-md/md.h"

namespace
{
    const kdb::RegisterCommand
        kdbBootinfo("bootinfo", "Display boot info", [](int, const kdb::Argument*) {
            if (bootinfo == NULL) {
                kprintf("no bootinfo supplied by loader\n");
                return;
            }
            kprintf("modules location   : %x\n", bootinfo->bi_modules);
            kprintf(
                "memorymap location: %x - %x\n", bootinfo->bi_memory_map_addr,
                bootinfo->bi_memory_map_addr + bootinfo->bi_memory_map_size);
        });

    const kdb::RegisterCommand kdbReboot("reboot", "Force a reboot", [](int, const kdb::Argument*) {
        md::Reboot();
    });

} // unnamed namespace

/* vim:set ts=2 sw=2: */
