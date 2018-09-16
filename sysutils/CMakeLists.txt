project(sysutils)
cmake_minimum_required(VERSION 3.9)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -I${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(hierarchy)
add_subdirectory(init)
add_subdirectory(getty)
add_subdirectory(ldd)
add_subdirectory(ps)
add_subdirectory(etc)
add_subdirectory(mount)
add_subdirectory(umount)
