# InstallSymlink is inspired by https://github.com/bro/cmake/blob/master/InstallSymlink.cmake,
# but we don't care about CMake < 3.7 anyway
macro(InstallSymlink _path _link)
	get_filename_component(_lname ${_link} NAME)
	get_filename_component(_lpath ${_link} PATH)
	execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${_path} ${CMAKE_CURRENT_BINARY_DIR}/${_lname})
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_lname} DESTINATION ${CMAKE_INSTALL_PREFIX}/${_lpath})
endmacro(InstallSymlink)

install(DIRECTORY DESTINATION ankh)
InstallSymlink(ankh/dev dev)
InstallSymlink(ankh/proc proc)
install(DIRECTORY DESTINATION tmp)
