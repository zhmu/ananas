cmake_minimum_required(VERSION 3.9)
project(awterm)

add_subdirectory(teken)

add_executable(awterm main.cpp)
target_include_directories(awterm PRIVATE ../awm)
target_link_libraries(awterm PRIVATE awe teken)
install(TARGETS awterm DESTINATION usr/bin)
