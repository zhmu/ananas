project(newlib)

cmake_minimum_required(VERSION 3.9)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${CMAKE_SOURCE_DIR}/build/targ-include")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPACKAGE_NAME=\"newlib\" -DPACKAGE_TARNAME=\"newlib\" -DPACKAGE_VERSION=\"4.1.0\" -DPACKAGE_STRING=\"newlib\ 4.1.0\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\"")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin -DHAVE_INIT_FINI")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMISSING_SYSCALL_NAMES")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g") # XXX

add_subdirectory(newlib/libc)
add_subdirectory(newlib/libm)
