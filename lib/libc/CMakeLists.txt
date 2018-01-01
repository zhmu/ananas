project(libc C ASM)
cmake_minimum_required(VERSION 3.9)

set(PDCLIB_THREADLIB nothread)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_PDCLIB_BUILD")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SOURCE_DIR}/includes -I${CMAKE_SOURCE_DIR}/internals")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SOURCE_DIR}/../../include") # for ananas/ XXX why do we need this?
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SOURCE_DIR}/platform/ananas/includes -I${CMAKE_SOURCE_DIR}/platform/ananas/internals")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SOURCE_DIR}/opt/${PDCLIB_THREADLIB}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-builtin-requires-header")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) # XXX why is this necessary?

add_subdirectory(3rdparty)
add_subdirectory(opt/${PDCLIB_THREADLIB})
add_subdirectory(platform/ananas)
add_subdirectory(platform/ananas/arch/amd64)
add_subdirectory(functions)
add_subdirectory(includes)
add_subdirectory(internals)

set(BUILD_SOURCES ${SOURCES})
add_library(c SHARED ${BUILD_SOURCES})
target_link_libraries(c syscall)

install(TARGETS c DESTINATION usr/lib)

install(FILES ${HEADERS}
	DESTINATION usr/include
)
