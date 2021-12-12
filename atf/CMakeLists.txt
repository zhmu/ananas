project(atf)
cmake_minimum_required(VERSION 3.9)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -I${CMAKE_CURRENT_SOURCE_DIR}/include -D_GNU_SOURCE")

add_subdirectory(../external/Catch2 catch2)

add_executable(atf signal.cpp wait.cpp)
target_link_libraries(atf PRIVATE Catch2::Catch2WithMain)

install(TARGETS atf DESTINATION usr/bin)
