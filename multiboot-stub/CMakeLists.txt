project(mb-stub ASM C)
cmake_minimum_required(VERSION 3.9)

set(COMMON_FLAGS "${COMMON_FLAGS} -m32 -O2 -mno-red-zone -mno-sse -I../include -DLOADER -Wall")
set(COMMON_FLAGS "${COMMON_FLAGS} -DDEBUG=0")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -DASM ${COMMON_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${COMMON_FLAGS}")

set(CMAKE_C_LINK_EXECUTABLE "ld <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_C_LINK_FLAGS "-b elf32-i386 -T ${CMAKE_CURRENT_SOURCE_DIR}/ldscript -nostdlib -nodefaultlibs")

set(SOURCES
	stub.S
	amd64.c
	bootinfo.c
	io.c
	lib.c
	main.c
	relocate.c
	verify.c
)

add_executable(mbstub ${SOURCES})

set_target_properties(mbstub PROPERTIES OUTPUT_NAME "mbstub.bin")
install(TARGETS mbstub DESTINATION lib)
