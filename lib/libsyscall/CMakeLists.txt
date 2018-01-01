project(syscall ASM)
cmake_minimum_required(VERSION 3.9)

# XXX so we can include syscall.gen.S
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_BINARY_DIR}")

# XXX this should be made per-platform, if we ever have more than one
set(SOURCES
	amd64/syscall.S
)
add_library(syscall STATIC ${SOURCES})

# generate syscall.gen.S from the kern's syscalls.in - we don't care about
# anything else so just write to /dev/null there
set(KERN_PATH ${CMAKE_SOURCE_DIR}/../../kern)
add_custom_command(
	OUTPUT syscall.gen.S
	COMMAND ${KERN_PATH}/gen_syscalls.sh ${KERN_PATH}/syscalls.in ${CMAKE_BINARY_DIR}/syscall.gen.S /dev/null /dev/null
	DEPENDS ${KERN_PATH}/syscalls.in
)

# ensure syscall.gen.S is created before we try to build our library
add_custom_target(syscall_gen DEPENDS syscall.gen.S)
add_dependencies(syscall syscall_gen)

install(TARGETS syscall DESTINATION usr/lib)
