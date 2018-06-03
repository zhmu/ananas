add_executable(umount umount.cpp)
target_link_libraries(umount unwind c++)
install(TARGETS umount DESTINATION sbin)
