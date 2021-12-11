project(crt C ASM)
cmake_minimum_required(VERSION 3.9)

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -fPIC)

add_library(crt1-common OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/common/crt1-common.c)
add_library(crt1S OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/amd64/crt1.S)

# crt1 consists of the common crt1 stuff + the platform specific bits; we merge them here
add_custom_target(crt1.o ALL
	DEPENDS crt1-common crt1S
	COMMAND ${CMAKE_LINKER} -r -o ${CMAKE_CURRENT_BINARY_DIR}/crt1.o $<TARGET_OBJECTS:crt1-common> $<TARGET_OBJECTS:crt1S>
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/crt1.o
	DESTINATION usr/lib
)
