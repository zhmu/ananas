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
	ioapic.cpp
	pic.cpp
	pit.cpp
	smp.cpp
)

if(OPTION_GDB)
	add_sources(gdb-support.cpp)
endif()

if(OPTION_DEBUG_CONSOLE)
	add_sources(debug-console.cpp)
endif()
