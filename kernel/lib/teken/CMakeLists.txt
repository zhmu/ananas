add_sources(
	teken.c
)

# teken needs a teken_state.h file which is generated; force it to be generated here
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/include/teken_state.h
	COMMAND awk -f ${CMAKE_CURRENT_SOURCE_DIR}/gensequences ${CMAKE_CURRENT_SOURCE_DIR}/sequences > ${CMAKE_BINARY_DIR}/include/teken_state.h
)
add_custom_target(teken_state_h ALL DEPENDS ${CMAKE_BINARY_DIR}/include/teken_state.h)
