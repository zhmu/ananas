#
# Word of warning: this is an extremely kludgy hack, but I can't get the
# dependencies right - we want to build object files and there's no nice
# way to do that... improvements are welcome.
#
project(crt ASM)
cmake_minimum_required(VERSION 3.9)

function(build_object dest src opts)
	add_custom_target(${dest} ALL
		COMMAND ${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS} ${opts} -c -o ${CMAKE_CURRENT_BINARY_DIR}/${dest} ${CMAKE_CURRENT_SOURCE_DIR}/${src} ${opts}
	)
endfunction()

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} --sysroot=${CMAKE_SYSROOT})

build_object(crt0-common.o common/crt0-common.c "")
build_object(crt0.S.o amd64/crt0.S "")
build_object(crti.o amd64/crti.S "")
build_object(crtbegin.o amd64/crtbegin.S "")
build_object(crtbeginS.o amd64/crtbegin.S "-DSHARED")
build_object(crtend.o amd64/crtend.S "")
build_object(crtendS.o amd64/crtend.S "-DSHARED")
build_object(crtn.o amd64/crtn.S "")

add_custom_target(crt0.o ALL
	DEPENDS crt0-common.o crt0.S.o
	COMMAND ${CMAKE_LD_LINKER} -r -o ${CMAKE_CURRENT_BINARY_DIR}/crt0.o ${CMAKE_CURRENT_BINARY_DIR}/crt0-common.o ${CMAKE_CURRENT_BINARY_DIR}/crt0.S.o
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/crt0.o
	${CMAKE_CURRENT_BINARY_DIR}/crti.o
	${CMAKE_CURRENT_BINARY_DIR}/crtbeginS.o
	${CMAKE_CURRENT_BINARY_DIR}/crtbegin.o
	${CMAKE_CURRENT_BINARY_DIR}/crtendS.o
	${CMAKE_CURRENT_BINARY_DIR}/crtend.o
	${CMAKE_CURRENT_BINARY_DIR}/crtn.o
	DESTINATION usr/lib
)
