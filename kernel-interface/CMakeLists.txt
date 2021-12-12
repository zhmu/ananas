project(kernel-interface NONE)
cmake_minimum_required(VERSION 3.9)

# kernel headers
install(DIRECTORY ananas
    DESTINATION usr/include
    FILES_MATCHING PATTERN "*.h"
)

# generate syscalls.h from the kern's syscalls.in - we don't care about
# anything else so just write to /dev/null there
set(KERN_PATH ${CMAKE_SOURCE_DIR}/../kern)
add_custom_command(
	OUTPUT syscalls.h
	COMMAND ${KERN_PATH}/gen_syscalls.sh ${KERN_PATH}/syscalls.in /dev/null ${CMAKE_BINARY_DIR}/syscalls.h /dev/null
	DEPENDS ${KERN_PATH}/syscalls.in
)
add_custom_target(${PROJECT_NAME} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/syscalls.h)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syscalls.h DESTINATION usr/include/_gen)
