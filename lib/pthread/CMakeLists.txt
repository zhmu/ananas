project(pthread)
cmake_minimum_required(VERSION 3.9)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SOURCE_DIR}/include -I${CMAKE_BINARY_DIR}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -ffreestanding -fno-builtin")

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) # XXX why is this necessary?

add_subdirectory(src)
add_subdirectory(include)

add_library(pthread SHARED ${SOURCES})
target_link_libraries(pthread -nostdlib)

install(TARGETS pthread DESTINATION usr/lib)
