cmake_minimum_required(VERSION 3.9)
project(awm)

add_definitions(-std=c++2a)
add_executable(awm main.cpp font.cpp window.cpp windowmanager.cpp socketserver.cpp pixelbuffer.cpp)

if(USE_SDL2)
find_package(SDL2 REQUIRED)

target_sources(awm PRIVATE platform_sdl2.cpp)
target_include_directories(awm PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(awm ${SDL2_LIBRARIES})
else()
target_sources(awm PRIVATE platform_ananas.cpp)
endif()
install(TARGETS awm DESTINATION usr/bin)
install(FILES data/Roboto-Regular.ttf DESTINATION usr/share/fonts)
install(FILES data/RobotoMono-Regular.ttf DESTINATION usr/share/fonts)
