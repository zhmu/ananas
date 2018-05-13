project(crt C ASM)
cmake_minimum_required(VERSION 3.9)

# install an object file as name.o (would be name.S.o otherwise)
function(build_object name src)
	add_library(${name} OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/${src})
	install(FILES
		$<TARGET_OBJECTS:${name}>
		RENAME ${name}.o
		DESTINATION usr/lib
	)
endfunction()

add_library(crt0-common OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/common/crt0-common.c)
add_library(crt0S OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/amd64/crt0.S)

build_object(crti amd64/crti.S)
build_object(crtbegin amd64/crtbegin.S)
build_object(crtbeginS amd64/crtbegins.S)
build_object(crtend amd64/crtend.S)
build_object(crtendS amd64/crtends.S)
build_object(crtn amd64/crtn.S)

# crt0 consists of the common crt0 stuff + the platform specific bits; we merge them here
add_custom_target(crt0.o ALL
	DEPENDS crt0-common crt0S
	COMMAND ${CMAKE_LINKER} -r -o ${CMAKE_CURRENT_BINARY_DIR}/crt0.o $<TARGET_OBJECTS:crt0-common> $<TARGET_OBJECTS:crt0S>
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/crt0.o
	DESTINATION usr/lib
)
