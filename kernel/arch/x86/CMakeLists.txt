add_sources(
	pic.cpp
	pit.cpp
	exceptions.cpp
)

if(OPTION_DEBUG_CONSOLE)
	add_sources(debug-console.cpp)
endif()

if(OPTION_SMP)
	add_sources(
		acpi-smp.cpp
		smp.cpp
		ioapic.cpp
	)
endif()
