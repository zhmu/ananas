add_sources(
	stub.S
    asmsyms.cpp
	md_map.cpp
	md_thread.cpp
	md_vmspace.cpp
	mp_stub.S
	startup.cpp
	interrupts.S
	exception.cpp
	reboot.cpp
	uinit.S
	usupport.S
	ioapic.cpp
	pic.cpp
	pit.cpp
	smp.cpp
)

add_sources(gdb-support.cpp)
add_sources(debug-console.cpp)

add_subdirectory(loader)
