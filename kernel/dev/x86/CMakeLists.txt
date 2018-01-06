if(DEVICE_ATKBD)
	add_sources(atkbd.cpp)
endif()

if(DEVICE_ATRTC)
	add_sources(atrtc.cpp)
endif()

if(DEVICE_VGA)
	add_sources(vga.cpp)
endif()

if(DEVICE_SIO)
	add_sources(sio.cpp)
endif()
