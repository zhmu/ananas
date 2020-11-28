project(loader ASM)

# pass -divide to the assembler so we can use '/' in the sources; without
# this it'll be parsed as a comment, making it impossible to divide. We only
# need to worry about it for our own as(1) since it's disabled for Linux and
# BSD in gas/config/tc-i386.c
set(CMAKE_ASM_FLAGS "-m32 -Xassembler -divide")
set(CMAKE_ASM_LINK_EXECUTABLE "ld <CMAKE_ASM_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_ASM_LINK_FLAGS "-b elf32-i386 -T ${CMAKE_CURRENT_SOURCE_DIR}/ldscript -nostdlib -nodefaultlibs")

add_executable(multiboot multiboot.S)
set_target_properties(multiboot PROPERTIES OUTPUT_NAME "multiboot.bin")
