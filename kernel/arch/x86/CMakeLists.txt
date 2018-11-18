add_sources(
	ioapic.cpp
	pic.cpp
	pit.cpp
	exceptions.cpp
	smp.cpp
)

if(OPTION_DEBUG_CONSOLE)
	add_sources(debug-console.cpp)
endif()
