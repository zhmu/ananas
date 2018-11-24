add_sources(
	stub.S
	md_map.cpp
	md_thread.cpp
	md_vmspace.cpp
	mp_stub.S
	startup.cpp
	interrupts.S
	exception.cpp
	reboot.cpp
	usupport.S
)

if(OPTION_GDB)
	add_sources(gdb-support.cpp)
endif()
