if(DEVICE_HDA)
	add_sources(
		hda.cpp
		hda-codec.cpp
		hda-routing.cpp
		hda-pci.cpp
	)
endif()
