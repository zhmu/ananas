add_executable(init init.cpp ttys.cpp)
target_link_libraries(init unwind c++)
install(TARGETS init DESTINATION sbin)
