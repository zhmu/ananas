project(libm)
cmake_minimum_required(VERSION 3.9)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
# avoid hiding M_PI_2 and such
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_BSD_SOURCE")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SOURCE_DIR}/include -I${CMAKE_SOURCE_DIR}/internal")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I.") # for math-bits
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -ffreestanding -fno-builtin")
# XXX some files need this to build
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas -Wno-shift-op-parentheses")

add_subdirectory(complex)
add_subdirectory(src)
add_subdirectory(fenv)
add_subdirectory(include)
add_subdirectory(arch/amd64)

# now filter everything that appears in MD_SOURCES from SOURCES
set(BUILD_SOURCES ${SOURCES})
foreach(f ${MD_SOURCES})
	get_filename_component(r ${f} NAME_WE)
	list(FILTER BUILD_SOURCES EXCLUDE REGEX "/${r}.c$")
endforeach(f)
set(BUILD_SOURCES ${BUILD_SOURCES} ${MD_SOURCES})

add_library(m SHARED ${BUILD_SOURCES})

# we need a symlink to our machine-specific stuff, so force that here
add_custom_target(
	m_machine_symlink ALL
	COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/include/amd64 ${CMAKE_BINARY_DIR}/math-bits
)
add_dependencies(m m_machine_symlink)

install(TARGETS m DESTINATION usr/lib)
install(FILES ${HEADERS}
	DESTINATION usr/include
)
install(FILES ${MD_HEADERS}
	DESTINATION usr/include/math-bits
)
