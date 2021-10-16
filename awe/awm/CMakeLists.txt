cmake_minimum_required(VERSION 3.9)
project(awm)

add_executable(awm main.cpp window.cpp windowmanager.cpp socketserver.cpp)
target_link_libraries(awm awe)

if(USE_SDL2)
find_package(SDL2 REQUIRED)

target_sources(awm PRIVATE platform_sdl2.cpp)
target_include_directories(awm PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(awm ${SDL2_LIBRARIES})
else()
target_sources(awm PRIVATE platform_ananas.cpp)
endif()
install(TARGETS awm DESTINATION usr/bin)
