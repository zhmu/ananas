add_executable(getty getty.cpp)
target_link_libraries(getty unwind c++)
install(TARGETS getty DESTINATION usr/libexec)
