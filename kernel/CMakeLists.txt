project(kernel ASM C CXX)
cmake_minimum_required(VERSION 3.9)

set(COMMON_FLAGS "${COMMON_FLAGS} -fno-rtti -fno-exceptions")
# ensure we don't drag in external things
set(COMMON_FLAGS "${COMMON_FLAGS} -fno-builtin -nostdlib -nostdinc")
set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/../include") # for global includes
set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/include") # for kernel/
set(COMMON_FLAGS "${COMMON_FLAGS} -I${CMAKE_CURRENT_BINARY_DIR}/include")
set(COMMON_FLAGS "${COMMON_FLAGS} -DKERNEL")

# machine-dependent flags
set(COMMON_FLAGS "${COMMON_FLAGS} -m64 -march=athlon64 -mcmodel=large")
set(COMMON_FLAGS "${COMMON_FLAGS} -mno-red-zone -mno-sse -fno-stack-protector")

set(COMMON_FLAGS "${COMMON_FLAGS} -O2 -g")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -DASM ${COMMON_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ${COMMON_FLAGS}")

set(CMAKE_CXX_LINK_EXECUTABLE "ld <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_FLAGS "-b elf64-x86-64 -T ${CMAKE_CURRENT_SOURCE_DIR}/conf/ld.amd64 -nostdlib -nodefaultlibs")

function(build_object dest src opts)
	string(REPLACE " " ";" flags ${CMAKE_CXX_FLAGS})
	add_custom_command(
		OUTPUT ${dest}
		COMMAND ${CMAKE_CXX_COMPILER} ${flags} ${opts} -c -o ${CMAKE_CURRENT_BINARY_DIR}/${dest} ${CMAKE_CURRENT_SOURCE_DIR}/${src} ${opts}
	)
endfunction()

# inspired by https://stackoverflow.com/questions/31538466/keeping-file-hierarchy-across-subdirectories-in-cmake
function(add_sources)
	foreach(_s IN ITEMS ${ARGV})
		get_filename_component(_a "${_s}" ABSOLUTE)
		set_property(GLOBAL APPEND PROPERTY SourceList "${_a}")
	endforeach()
endfunction()

# XXX for now, hardcode all the options
set(OPTION_KDB 1)
set(OPTION_GDB 1)
set(OPTION_DEBUG_CONSOLE 1)
set(OPTION_SMP 1)
set(OPTION_ZLIB 1)

set(DEVICE_AHCI 1)
set(DEVICE_ATA 1)
set(DEVICE_ATKBD 1)
set(DEVICE_ATRTC 1)
set(DEVICE_HDA 1)
set(DEVICE_OHCI 1)
set(DEVICE_PCI 1)
set(DEVICE_SATA 1)
set(DEVICE_SCSI 1)
set(DEVICE_SIO 1)
set(DEVICE_UHCI 1)
set(DEVICE_USB 1)
set(DEVICE_VGA 1)

set(FS_ANKHFS 1)
set(FS_CRAMFS 1)
set(FS_EXT2 1)
set(FS_FAT 1)
set(FS_ISO9660 1)

#
add_subdirectory(arch)
add_subdirectory(dev)
add_subdirectory(fs)
add_subdirectory(gdb)
add_subdirectory(kdb)
add_subdirectory(kern)
add_subdirectory(lib)
add_subdirectory(sys)
add_subdirectory(vfs)
add_subdirectory(vm)

get_property(SOURCES GLOBAL PROPERTY SourceList)
set(SOURCES ${SOURCES} include/_gen/syscalls.h)
add_executable(kernel_elf ${SOURCES})
set_target_properties(kernel_elf PROPERTIES OUTPUT_NAME "kernel.elf")

# add a custom target that combines the multiboot stub to get a bootable kernel
# XXX this will only work on unix-like systems
add_custom_command(
	OUTPUT kernel
	COMMENT "Creating kernel binary"
	DEPENDS kernel_elf
	COMMAND cat ${CMAKE_SYSROOT}/lib/mbstub.bin ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf > ${CMAKE_CURRENT_BINARY_DIR}/kernel
)
add_custom_target(kernel_bin ALL DEPENDS kernel)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kernel DESTINATION .)

# prepare our include directories: we make a symlink to avoid unnecessary
# rebuilds
add_custom_target(kernel_md_param_h ALL
	COMMAND ${CMAKE_COMMAND} -E create_symlink
	${CMAKE_CURRENT_SOURCE_DIR}/include/kernel/amd64
	${CMAKE_CURRENT_BINARY_DIR}/include/kernel-md
)
add_dependencies(kernel_elf kernel_md_param_h)
add_custom_target(machine_types_h ALL
	COMMAND ${CMAKE_COMMAND} -E create_symlink
	${CMAKE_CURRENT_SOURCE_DIR}/../include/ananas/amd64
	${CMAKE_CURRENT_BINARY_DIR}/include/machine
)
add_dependencies(kernel_elf machine_types_h)

# generate syscalls.h from the kern's syscalls.in - we don't care about
# anything else so just write to /dev/null there
set(KERN_PATH ${CMAKE_SOURCE_DIR}/../kern)
add_custom_command(
	OUTPUT include/_gen/syscalls.h
	COMMAND ${KERN_PATH}/gen_syscalls.sh ${KERN_PATH}/syscalls.in /dev/null ${CMAKE_BINARY_DIR}/include/_gen/syscalls.h /dev/null
	DEPENDS ${KERN_PATH}/syscalls.in
)
#add_custom_target(syscall_gen DEPENDS include/_gen/syscalls.h)
#add_dependencies(kernel_elf syscall_gen)

# generate asmsyms.h; this is needed to look up offsets within structs from
# assembler files, which is sure a lot better than having to maintain them
# ourselves
build_object(asmsyms.o arch/amd64/asmsyms.cpp "")
add_custom_command(
	OUTPUT include/asmsyms.h
	COMMAND ${CMAKE_SOURCE_DIR}/tools/gen_asmsyms.sh nm awk ${CMAKE_CURRENT_BINARY_DIR}/asmsyms.o > include/asmsyms.h
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/asmsyms.o
)
add_custom_target(asmsyms_h DEPENDS include/asmsyms.h)
add_dependencies(kernel_elf asmsyms_h)

# XXX this should be done automatically
set(OPT_H ${CMAKE_CURRENT_BINARY_DIR}/include/options.h)
file(WRITE  ${OPT_H} "#define ARCHITECTURE \"amd64\"\n")
file(APPEND ${OPT_H} "#define OPTION_DEBUG_CONSOLE\n")
file(APPEND ${OPT_H} "#define OPTION_SMP\n")
file(APPEND ${OPT_H} "#define OPTION_ANKHFS\n")
file(APPEND ${OPT_H} "#define OPTION_KDB\n")
file(APPEND ${OPT_H} "#define OPTION_ACPI\n")
file(APPEND ${OPT_H} "#define FS_ANKHFS\n")
