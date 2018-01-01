project(ld-ananas ASM CXX)
cmake_minimum_required(VERSION 3.9)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -fno-builtin")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
# hide all symbols by default so we won't accidently publish things applications can use
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) # XXX why is this necessary?

# force symbols to remain within the loader
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
# force sysv-style hash as we don't support anything else yet
# XXX we should force this in the toolchain
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--hash-style=sysv")
# prevent linking with standard libraries (i.e. libc), we are responsible for loading them
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -nostdlib")
# force our entry point
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -e entry")

add_subdirectory(amd64)
add_subdirectory(common)

add_library(ld-ananas SHARED ${SOURCES})
target_link_libraries(ld-ananas syscall)

set_target_properties(ld-ananas PROPERTIES PREFIX "") # remove lib-prefix
install(TARGETS ld-ananas DESTINATION lib)
