#ifndef ANANAS_AMD64_GDB_SUPPORT_H
#define ANANAS_AMD64_GDB_SUPPORT_H

#define GDB_NUMREGS 56
#define GDB_REG_PC 16

size_t gdb_md_get_register_size(int regnum);
void* gdb_md_get_register(struct STACKFRAME* sf, int regnum);
int gdb_md_map_signal(struct STACKFRAME* sf);

#endif // ANANAS_AMD64_GDB_SUPPORT_H
