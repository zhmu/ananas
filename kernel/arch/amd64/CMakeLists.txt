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

add_sources(gdb-support.cpp)
add_sources(debug-console.cpp)

add_subdirectory(loader)
