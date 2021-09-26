cmake_minimum_required(VERSION 3.9)
project(awterm)

add_subdirectory(teken)

add_definitions(-std=c++2a)
add_executable(awterm main.cpp ../awm/font.cpp ../awm/pixelbuffer.cpp)
target_include_directories(awterm PRIVATE ../awm)
target_link_libraries(awterm PRIVATE teken)
install(TARGETS awterm DESTINATION usr/bin)
