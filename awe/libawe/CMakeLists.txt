project(libawe)
cmake_minimum_required(VERSION 3.9)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(awe
    src/connection.cpp
    src/font.cpp
    src/pixelbuffer.cpp
    src/types.cpp
    src/window.cpp
)
target_include_directories(awe PUBLIC include)

install(TARGETS awe DESTINATION usr/lib)
install(FILES
    include/awe/connection.h
    include/awe/font.h
    include/awe/pixelbuffer.h
    include/awe/window.h
    include/awe/types.h
	DESTINATION usr/include/awe
)
