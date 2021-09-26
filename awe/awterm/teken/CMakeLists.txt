# teken needs a teken_state.h file which is generated; force it to be generated here
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/teken_state.h
	COMMAND awk -f ${CMAKE_CURRENT_SOURCE_DIR}/gensequences ${CMAKE_CURRENT_SOURCE_DIR}/sequences > ${CMAKE_CURRENT_BINARY_DIR}/teken_state.h
)
add_library(teken teken.c ${CMAKE_CURRENT_BINARY_DIR}/teken_state.h)
target_include_directories(teken
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
