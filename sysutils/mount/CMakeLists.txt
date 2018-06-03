add_executable(mount mount.cpp)
target_link_libraries(mount unwind c++)
install(TARGETS mount DESTINATION sbin)
