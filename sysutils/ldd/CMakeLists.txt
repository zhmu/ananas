add_executable(ldd ldd.cpp)
target_link_libraries(ldd unwind c++)
install(TARGETS ldd DESTINATION usr/bin)
