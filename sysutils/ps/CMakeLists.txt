add_executable(ps ps.cpp)
target_link_libraries(ps unwind c++)
install(TARGETS ps DESTINATION bin)
