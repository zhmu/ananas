project(loader ASM)

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32")
set(CMAKE_ASM_LINK_EXECUTABLE "ld <CMAKE_ASM_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_ASM_LINK_FLAGS "-b elf32-i386 -T ${CMAKE_CURRENT_SOURCE_DIR}/ldscript -nostdlib -nodefaultlibs")

add_executable(multiboot multiboot.S)
set_target_properties(multiboot PROPERTIES OUTPUT_NAME "multiboot.bin")
