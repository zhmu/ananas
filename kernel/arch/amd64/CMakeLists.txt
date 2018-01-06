add_sources(
	stub.S
	md_map.cpp
	md_thread.cpp
	md_vmspace.cpp
	startup.cpp
	interrupts.S
	exception.cpp
	reboot.cpp
)

if(OPTION_SMP)
	add_sources(mp_stub.S)
endif()

if(OPTION_GDB)
	add_sources(gdb-support.cpp)
endif()
