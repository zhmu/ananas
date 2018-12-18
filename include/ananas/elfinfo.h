#ifndef ANANAS_ELFINFO_H
#define ANANAS_ELFINFO_H

#include <ananas/types.h>

struct ANANAS_ELF_INFO {
    // Length of this struct
    size_t ei_size;
    // Base address where we relocated the interpreter
    addr_t ei_interpreter_base;
    // Virtual address of the program header
    addr_t ei_phdr;
    // Number of program header entries
    size_t ei_phdr_entries;
    // Entry point
    addr_t ei_entry;
};

#endif /* ANANAS_ELFINFO_H */
