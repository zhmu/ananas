if(DEVICE_VCONSOLE)
	add_sources(
		framebuffer.cpp
		vconsole.cpp
		vga.cpp
		vtty.cpp
	)
endif()
